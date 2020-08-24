#include "../include/decision_tree.h"

#include "../include/memory.h"

toccata::DecisionTree::DecisionTree() {
    m_library = nullptr;
    m_segment = nullptr;
    m_threadCount = 0;
}

toccata::DecisionTree::~DecisionTree() {
    /* void */
}

void toccata::DecisionTree::OnNoteChange(int changedNote) {
    std::vector<Decision *> newDecisionList;

    const int decisionCount = GetDecisionCount();
    for (int i = 0; i < decisionCount; ++i) {
        bool valid = true;
        for (int note : m_decisions[i]->Notes) {
            if (note >= changedNote) {
                valid = false;
                break;
            }
        }

        if (valid) {
            newDecisionList.push_back(m_decisions[i]);
        }
    }

    m_decisions = newDecisionList;
}

void toccata::DecisionTree::Initialize(int threadCount) {
    m_threadCount = threadCount;
    m_threadContexts = Memory::Allocate<ThreadContext>(m_threadCount);

    for (int i = 0; i < m_threadCount; ++i) {
        m_threadContexts[i].Solver.Initialize();
    }
}

void toccata::DecisionTree::SpawnThreads() {
    if (m_threadCount > 1 || ForceMultithreaded) {
        for (int i = 0; i < m_threadCount; ++i) {
            m_threadContexts[i].Thread = new std::thread(&DecisionTree::WorkerThread, this, i);
        }
    }
    else if (m_threadCount == 1) {
        m_threadContexts[0].Thread = nullptr;
    }
}

void toccata::DecisionTree::KillThreads() {
    for (int i = 0; i < m_threadCount; ++i) {
        m_threadContexts[i].Kill = true;
    }

    TriggerThreads();

    if (m_threadCount > 1 || ForceMultithreaded) {
        for (int i = 0; i < m_threadCount; ++i) {
            m_threadContexts[i].Thread->join();
            delete m_threadContexts[i].Thread;

            m_threadContexts[i].Thread = nullptr;
        }
    }
}

void toccata::DecisionTree::Destroy() {
    for (int i = 0; i < m_threadCount; ++i) {
        assert(m_threadContexts[i].Thread == nullptr);
    }

    for (int i = 0; i < m_threadCount; ++i) {
        m_threadContexts[i].Solver.Release();
    }

    Memory::Free(m_threadContexts);

    m_threadContexts = nullptr;
}

void toccata::DecisionTree::Process(int startIndex) {
    for (int i = 0; i < m_threadCount; ++i) {
        m_threadContexts[i].StartIndex = startIndex;
    }

    DistributeWork();
    TriggerThreads();
    WaitForThreads();

    Integrate();
}

void toccata::DecisionTree::DistributeWork() {
    if (m_library != nullptr) {
        int libraryStart = 0;
        const int libraryBars = m_library->GetBarCount();
        const int libraryDelta = (int)std::ceil(libraryBars / (double)m_threadCount);
        for (int i = 0; i < m_threadCount; ++i) {
            int end = libraryStart + libraryDelta - 1;
            if (end >= libraryBars) end = libraryBars - 1;

            m_threadContexts[i].LibraryStart = libraryStart;
            m_threadContexts[i].LibraryEnd = end;

            libraryStart = end + 1;
        }
    }
    else {
        for (int i = 0; i < m_threadCount; ++i) {
            m_threadContexts[i].LibraryStart = 0;
            m_threadContexts[i].LibraryEnd = -1;
        }
    }

    int decisionStart = 0;
    const int decisionCount = GetDecisionCount();
    const int decisionDelta = (int)std::ceil(decisionCount / (double)m_threadCount);
    for (int i = 0; i < m_threadCount; ++i) {
        int end = decisionStart + decisionDelta - 1;
        if (end >= decisionCount) end = decisionCount - 1;

        m_threadContexts[i].DecisionStart = decisionStart;
        m_threadContexts[i].DecisionEnd = end;

        decisionStart = end + 1;
    }
}

void toccata::DecisionTree::TriggerThreads() {
    if (m_threadCount == 1 && !ForceMultithreaded) {
        Work(0, m_threadContexts[0]);
    }
    else {
        for (int i = 0; i < m_threadCount; ++i) {
            std::lock_guard<std::mutex> lk(m_threadContexts[i].Lock);
            m_threadContexts[i].Trigger = true;
            m_threadContexts[i].ConditionVariable.notify_one();
        }
    }
}

void toccata::DecisionTree::WaitForThreads() {
    if (m_threadCount == 1 && !ForceMultithreaded) {
        return;
    }

    for (int i = 0; i < m_threadCount; ++i) {
        std::unique_lock<std::mutex> lk(m_threadContexts[i].Lock);
        m_threadContexts[i].ConditionVariable
            .wait(lk, [this, i] { return m_threadContexts[i].Done; });

        m_threadContexts[i].Done = false;
    }
}

void toccata::DecisionTree::Integrate() {
    for (int i = 0; i < m_threadCount; ++i) {
        std::queue<Decision *> &newDecisions = m_threadContexts[i].NewDecisions;

        while (!newDecisions.empty()) {
            Decision *decision = newDecisions.front(); newDecisions.pop();

            if (!IntegrateDecision(decision)) {
                DestroyDecision(decision);
            }
        }
    }
}

int toccata::DecisionTree::GetDepth(Decision *decision) const {
    if (!decision->IsCached()) {
        Decision *bestParent = FindBestParent(decision);

        if (bestParent != nullptr) {
            decision->ParentDecision = bestParent;
            decision->Depth = 1 + GetDepth(bestParent);
        }
        else {
            decision->ParentDecision = nullptr;
            decision->Depth = 1;
        }

        decision->Cached = true;
    }

    return decision->Depth;
}

toccata::DecisionTree::Decision *toccata::DecisionTree::FindBestParent(const Decision *decision) const {
    int bestDepth = -1;
    Decision *best = nullptr;

    const int decisions = GetDecisionCount();
    for (int i = 0; i < decisions; ++i) {
        Decision *prev = m_decisions[i];

        if (prev == decision) continue;
        else if (prev->GetEnd() < decision->GetEnd()) {
            if (!prev->MatchedBar->FindNext(decision->MatchedBar, 1) && 
                !decision->MatchedBar->FindNext(prev->MatchedBar, 1)) 
            {
                continue;
            }

            const int depth = GetDepth(prev);

            if (depth > bestDepth) {
                best = prev;
                bestDepth = depth;
            }
        }
    }

    return best;
}

bool toccata::DecisionTree::IntegrateDecision(Decision *decision) {
    const int decisionCount = GetDecisionCount();
    for (int i = 0; i < decisionCount; ++i) {
        Decision *currentDecision = m_decisions[i];

        const int minNoteCount = std::min(currentDecision->MappedNotes, decision->MappedNotes);
        const int overlap = (int)std::ceil(0.5 * minNoteCount);

        if (decision->Overlapping(currentDecision, overlap)) {
            if (decision->IsBetterFitThan(currentDecision)) {
                currentDecision->AverageError = decision->AverageError;
                currentDecision->MappedNotes = decision->MappedNotes;
                currentDecision->Notes = decision->Notes;
                currentDecision->s = decision->s;
                currentDecision->t = decision->t;
                currentDecision->MatchedBar = decision->MatchedBar;
            }

            return false;
        }
    }

    m_decisions.push_back(decision);
    return true;
}

void toccata::DecisionTree::WorkerThread(int threadId) {
    ThreadContext &context = m_threadContexts[threadId];

    while (!context.Kill) {
        std::unique_lock<std::mutex> lk(context.Lock);
        context.ConditionVariable
            .wait(lk, [this, threadId] { return m_threadContexts[threadId].Trigger; });

        context.Trigger = false;

        Work(threadId, context);

        context.Done = true;

        lk.unlock();
        context.ConditionVariable.notify_one();
    }
}

void toccata::DecisionTree::Work(int threadId, ThreadContext &context) {
    SeedMatch(context.LibraryStart, context.LibraryEnd, threadId);
}

void toccata::DecisionTree::SeedMatch(
    int libraryStart, int libraryEnd, int threadId) 
{
    ThreadContext &context = m_threadContexts[threadId];

    for (int i = libraryStart; i <= libraryEnd; ++i) {
        Bar *bar = m_library->GetBar(i);
        Decision *newDecision = Match(bar, context.StartIndex, context);

        if (newDecision != nullptr) {
            newDecision->ParentDecision = nullptr;
            context.NewDecisions.push(newDecision);
        }
    }
}

toccata::DecisionTree::Decision *toccata::DecisionTree::Match(
    const Bar *reference, int startIndex, ThreadContext &context) 
{
    const int n = reference->GetSegment()->NoteContainer.GetCount();
    const int k = m_segment->NoteContainer.GetCount();

    std::set<int> mappedNotes;

    FullSolver::Result result;
    result.Fit.Target = &mappedNotes;

    FullSolver::Request request;
    request.StartIndex = startIndex;
    request.EndIndex = startIndex + (int)std::ceil(n * (1.0 + m_margin)) - 1;
    if (request.EndIndex >= k) request.EndIndex = k - 1;
    request.Reference = reference->GetSegment();
    request.Segment = m_segment;

    bool foundSolution = context.Solver.Solve(request, &result);
    if (!foundSolution) return nullptr;

    Decision *newDecision = AllocateDecision();
    newDecision->AverageError = result.Fit.AverageError;
    newDecision->s = result.s;
    newDecision->t = result.t;
    newDecision->Notes = mappedNotes;
    newDecision->MappedNotes = result.Fit.MappedNotes;
    newDecision->MatchedBar = reference;

    return newDecision;
}

bool toccata::DecisionTree::Decision::IsSameAs(const Decision *decision) const {
    if (MatchedBar != decision->MatchedBar) return false;
    else return true;
}

bool toccata::DecisionTree::Decision::IsBetterFitThan(const Decision *decision) const {
    if (MappedNotes > decision->MappedNotes) return true;
    else if (MappedNotes < decision->MappedNotes) return false;

    const int footprint0 = decision->GetFootprint();
    const int footprint1 = decision->GetFootprint();

    if (footprint0 < footprint1) return true;
    else if (footprint1 > footprint0) return false;

    if (AverageError < decision->AverageError) return true;
    else return false;
}

bool toccata::DecisionTree::Decision::IsCached() const {
    if (!Cached) return false;
    
    if (ParentDecision != nullptr) {
        return ParentDecision->IsCached();
    }
    else return true;
}

void toccata::DecisionTree::Decision::InvalidateCache() {
    Cached = false;
    ParentDecision = nullptr;
    Depth = 0;
}

int toccata::DecisionTree::Decision::GetFootprint() const {
    return GetEnd() - GetStart() + 1;
}

int toccata::DecisionTree::Decision::GetEnd() const {
    return *(Notes.rbegin());
}

int toccata::DecisionTree::Decision::GetStart() const {
    return *Notes.begin();
}

bool toccata::DecisionTree::Decision::Overlapping(const Decision *decision, int overlap) const {
    int sharedNotes = 0;
    for (int n0 : Notes) {
        if (decision->Notes.count(n0) > 0) {
            if (++sharedNotes >= overlap) return true;
        }
    }

    return false;
}