// Demonstrates the compute module (Tensor / Kernel / Stream) for headless GPU
// computation. Runs a two-step pipeline:
//
//   1. SAXPY:  y[i] = 2 * x[i] + y[i]
//   2. ReLU:   y[i] = max(0, y[i])
//
// Input:  x[i] = i - N/2   (centred around zero)
//         y[i] = 0
// After SAXPY: y[i] = 2*(i - N/2)   negative for i < N/2, positive for i >= N/2
// After ReLU:  y[i] = 0             for i < N/2
//              y[i] = 2*(i - N/2)   for i >= N/2
//
// Contrast with 02-compute_example.cpp which does the same kind of work using
// the raw gpukit API — ~90 lines of boilerplate vs ~40 lines here.

#include <gpukit/compute/compute.h>
#include <gpukit/log.h>

#include <cmath>
#include <cstdint>
#include <vector>

bool compute_test() {
	bool ok = false;

	const uint32_t N = 1024; // must be a multiple of local_size_x (64)
	const int32_t half = static_cast<int32_t>(N / 2);

	// Build CPU-side data
	std::vector<float> x_data(N), y_data(N, 0.0f);
	for (uint32_t i = 0; i < N; ++i)
		x_data[i] = static_cast<float>(static_cast<int32_t>(i) - half);

	// Upload to GPU
	gpukit::Tensor<float> x(N), y(N);
	x.upload(x_data);
	y.upload(y_data);

	// Load kernels (GLSL compiled at runtime by shaderc)
	gpukit::Kernel saxpy("examples/assets/saxpy.comp");
	gpukit::Kernel relu("examples/assets/relu.comp");

	// Record and submit two chained dispatches.
	// Stream inserts a pipeline barrier between them automatically so the
	// ReLU dispatch sees the SAXPY result rather than the original y[].
	gpukit::Stream stream;
	stream.dispatch(saxpy, N, x, y); // y[i] = 2*x[i] + y[i]
	stream.dispatch(relu, N, y); // y[i] = max(0, y[i])
	stream.sync();

	// Read back and verify
	std::vector<float> result(N);
	y.download(result.data(), N);

	ok = true;
	for (uint32_t i = 0; i < N; ++i) {
		const float saxpy_val = 2.0f * x_data[i]; // + y_data[i] (= 0)
		const float expected = saxpy_val > 0.0f ? saxpy_val : 0.0f;
		if (std::fabs(result[i] - expected) > 1e-4f) {
			GPUKIT_LOG_ERROR("Mismatch at i={}: expected={} got={}", i, expected, result[i]);
			ok = false;
			break;
		}
	}

	GPUKIT_LOG_INFO("{} {} elements processed via SAXPY + ReLU.", ok ? "OK:" : "FAIL:", N);

	return ok;
}

int main() {
	gpukit::init({});

	bool ok = compute_test();

	gpukit::shutdown();

	return ok ? 0 : 1;
}
