#include <Tracy.hpp>

// ============================================================================
//
// - System -
//
// ============================================================================

#define PROF_INC_CTR(n) n++;
#define PROF_DUMP_CTR(n) TracyPlot(#n,n); n = 0;

struct CallCounter
{
    std::string m_time_name;
    std::string m_call_name;

    int64_t m_total_time = 0;
    int64_t m_total_calls = 0;

    CallCounter(std::string const& name)
        : m_time_name(name + "(time)")
        , m_call_name(name + "(calls)")
    {}

    void dump()
    {
        TracyPlot(m_time_name.c_str(), m_total_time);
        TracyPlot(m_call_name.c_str(), m_total_calls);
        m_total_time = 0;
        m_total_calls = 0;
    }
};

struct StackTimer
{
    std::chrono::system_clock::time_point m_start = std::chrono::system_clock::now();
    CallCounter* m_ctr;
    StackTimer(CallCounter* ctr)
        : m_start(std::chrono::system_clock::now())
        , m_ctr(ctr)
    {}

    ~StackTimer()
    {
        auto end = std::chrono::system_clock::now();
        uint64_t diff = std::chrono::duration_cast<std::chrono::nanoseconds>(end - m_start).count();
        m_ctr->m_total_calls++;
        m_ctr->m_total_time+=diff;
    }
};

#define PROF_SUM(n) StackTimer __counter(&n);
#define PROF_SUM_DUMP(n) n.dump();

// ============================================================================
//
// - Categories -
//
// ============================================================================

// Scopes
#define PROF_WORLD_SCOPE         ZoneScopedC(0x6ADEFC)
#define PROF_WORLD_SCOPE_N(n)    ZoneScopedNC(n,0x6ADEFC)

#define PROF_MAP_SCOPE           ZoneScopedC(0xFCD96A)
#define PROF_MAP_SCOPE_N(n)      ZoneScopedNC(n,0xFCD96A)

#define PROF_DATABASE_SCOPE      ZoneScopedC(0x80E66B)
#define PROF_DATABASE_SCOPE_N(n) ZoneScopedNC(n,0x80E66B)

// Call counters
extern CallCounter DELAYED_UNIT_RELOCATION_VISIT;
extern int UNIT_CTR;
