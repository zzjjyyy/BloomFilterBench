#define ANKERL_NANOBENCH_IMPLEMENT

#include <arrow/compute/exec/bloom_filter.h>
#include <nanobench.h>
#include <fmt/format.h>
#include <memory>

using namespace arrow::compute;

enum SimdOption {
  Scalar,
  AVX2
};

static constexpr int ITER = 3;

void makeBuildInput(int64_t num, uint64_t* hashes) {
  std::random_device rd;
  std::mt19937 gen(rd());
  auto dist = std::uniform_int_distribution<uint64_t>(0, UINT64_MAX);
  for (int64_t i = 0; i < num; ++i) {
    hashes[i] = dist(gen);
  }
}

void makeProbeInput(int64_t build_num, uint64_t* build_hashes, int64_t probe_num, uint64_t* probe_hashes, double sel) {
  // pick only a portion of build hashes as probe hashes to satisfy selectivity
  int64_t build_num_sel = build_num * sel;
  std::random_device rd;
  std::mt19937 gen(rd());
  auto dist = std::uniform_int_distribution<uint64_t>(0, build_num_sel - 1);
  for (int64_t i = 0; i < probe_num; ++i) {
    probe_hashes[i] = build_hashes[dist(gen)];
  }
}

std::shared_ptr<BlockedBloomFilter> build(int64_t num, const uint64_t* hashes, SimdOption simdOption) {
  int hardware_flags = arrow::internal::CpuInfo::GetInstance()->hardware_flags();
  if (simdOption == Scalar) {
    hardware_flags = 0;
  } else if (simdOption == AVX2) {
    hardware_flags |= arrow::internal::CpuInfo::AVX2;
  }
  auto builder = BloomFilterBuilder::Make(BloomFilterBuildStrategy::SINGLE_THREADED);
  auto bf = std::make_shared<BlockedBloomFilter>();
  auto status = builder->Begin(1, hardware_flags, arrow::default_memory_pool(), num, 1, bf.get());
  status = builder->PushNextBatch(0, num, hashes);
  return bf;
}

void probe(const std::shared_ptr<BlockedBloomFilter> &bf, int64_t num, const uint64_t* hashes,
           uint8_t* out, SimdOption simdOption) {
  int hardware_flags = arrow::internal::CpuInfo::GetInstance()->hardware_flags();
  if (simdOption == Scalar) {
    hardware_flags = 0;
  } else if (simdOption == AVX2) {
    hardware_flags |= arrow::internal::CpuInfo::AVX2;
  }
  bf->Find(hardware_flags, num, hashes, out, false);
}

void runBuild(int64_t num, const uint64_t* hashes) {
  ankerl::nanobench::Config().minEpochIterations(ITER).run(
          fmt::format("[Scalar] bf-build-{}-hashes", num), [&] {
            build(num, hashes, Scalar);
          });

  ankerl::nanobench::Config().minEpochIterations(ITER).run(
          fmt::format("[AVX-2] bf-build-{}-hashes", num), [&] {
            build(num, hashes, AVX2);
          });
}

void runProbe(int64_t num_build, const std::shared_ptr<BlockedBloomFilter> &bf,
              int64_t num_probe, const uint64_t* probe_hashes, uint8_t* out) {
  ankerl::nanobench::Config().minEpochIterations(ITER).run(
          fmt::format("[Scalar] bf-probe-{}-hashes (build={})", num_probe, num_build), [&] {
            probe(bf, num_probe, probe_hashes, out, Scalar);
          });

  ankerl::nanobench::Config().minEpochIterations(ITER).run(
          fmt::format("[AVX-2] bf-probe-{}-hashes (build={})", num_probe, num_build), [&] {
            probe(bf, num_probe, probe_hashes, out, AVX2);
          });
}

int main() {
  // build
  printf("\nBench bf-build:\n");
  const int num_tests = 6;
  int64_t nums[num_tests] = {1000, 10000, 100000, 1000000, 10000000, 100000000};
  uint64_t *build_data[num_tests];
  // data
  for (int i = 0; i < num_tests; ++i) {
    build_data[i] = new uint64_t[nums[i]];
    makeBuildInput(nums[i], build_data[i]);
  }
  // bench
  for (int i = 0; i < num_tests; ++i) {
    runBuild(nums[i], build_data[i]);
  }

  // probe (num-build = 10000)
  printf("\nBench bf-probe (num-build = 10000):\n");
  // data
  auto bf1 = build(nums[2], build_data[2], Scalar);
  const double sel = 0.1;
  uint64_t *probe_data1[num_tests];
  for (int i = 0; i < num_tests; ++i) {
    probe_data1[i] = new uint64_t[nums[i]];
    makeProbeInput(nums[2], build_data[2], nums[i], probe_data1[i], sel);
  }
  // bench
  for (int i = 0; i < num_tests; ++i) {
    uint8_t* out = new uint8_t[arrow::bit_util::BytesForBits(nums[i])];
    runProbe(nums[2], bf1, nums[i], probe_data1[i], out);
    delete[] out;
  }

  // probe (num-build = 10000000)
  printf("\nBench bf-probe (num-build = 10000000):\n");
  // data
  auto bf2 = build(nums[4], build_data[4], Scalar);
  uint64_t *probe_data2[num_tests];
  for (int i = 0; i < num_tests; ++i) {
    probe_data2[i] = new uint64_t[nums[i]];
    makeProbeInput(nums[4], build_data[4], nums[i], probe_data2[i], sel);
  }
  // bench
  for (int i = 0; i < num_tests; ++i) {
    uint8_t* out = new uint8_t[arrow::bit_util::BytesForBits(nums[i])];
    runProbe(nums[4], bf2, nums[i], probe_data2[i], out);
    delete[] out;
  }

  // free
  for (int i = 0; i < num_tests; ++i) {
    delete[] build_data[i];
    delete[] probe_data1[i];
    delete[] probe_data2[i];
  }
  return 0;
}