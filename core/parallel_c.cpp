#include "parallel_c.hpp"
#include "parallel.hpp"

static std::unique_ptr<Parallel> parallel;
static uint32_t worker_num;

void parallel_init(uint32_t num)
{
    // auto-select number of workers based on the number of cores
    if (num == 0) {
        num = std::thread::hardware_concurrency();
    }

    parallel = std::make_unique<Parallel>(num);

    worker_num = num;
}

void parallel_run(void task(uint32_t))
{
    parallel->run(task);
}

uint32_t parallel_worker_num()
{
    return worker_num;
}

void parallel_close()
{
    parallel.reset();
}
