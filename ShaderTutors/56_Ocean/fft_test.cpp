
#include "../common/gl4x.h"

void FFT_Test()
{
#if 0
	const int N = 32;

	Complex x[N];

	float sampling_freq = 4.0f / N;
	float var = 0.0f;

	for( int k = 0; k < N; ++k ) {
		x[k].a = sinf(GL_PI * var);
		var += sampling_freq;
	}
#else
	const int N = 16;

	Complex x[N] = {
		Complex(1, 0),
		Complex(2, -1),
		Complex(0, -1),
		Complex(-1, 2),
		Complex(3, 0),
		Complex(-2, 4),
		Complex(1, -3),
		Complex(0, 2),
		Complex(1, 2),
		Complex(-2, -1),
		Complex(5, -1),
		Complex(3, 4),
		Complex(4, -2),
		Complex(-4, 3),
		Complex(1, 0),
		Complex(-2, 5)
	};
#endif

	Complex X_dft[N];
	Complex X_fft[N];

	// DFT
	for( int k = 0; k < N; ++k ) {
		X_dft[k] = Complex(0, 0);

		for( int n = 0; n < N; ++n ) {
			float theta = (GL_2PI * k * n) / N;
			X_dft[k] += x[n] * Complex(cosf(theta), -sinf(theta));
		}
	}

	// FFT
	int log2_N = GLLog2OfPow2(N);

	for( int j = 0; j < N; ++j ) {
		int nj = (GLReverseBits32(j) >> (32 - log2_N)) & (N - 1);
		X_fft[nj] = x[j];
	}

#if 1
	// modified iterative algorithm for compute shader
	Complex T[N];
	Complex (*pingpong[2])[N] = { &X_fft, &T };
	int src = 0;

	for( int s = 1; s <= log2_N; ++s ) {
		int m = 1 << s;				// butterfly group height
		int mh = m >> 1;			// butterfly group half height

		// includes redundant butterfly calculations
		for( int l = 0; l < N; ++l ) {
			int k = (l * (N / m)) & (N - 1);
			int i = (l & ~(m - 1));	// butterfly group starting offset
			int j = (l & (mh - 1));	// butterfly index in group

			// twiddle factor W_N^k
			float theta = (GL_2PI * k) / (float)N;
			Complex W_N_k(cosf(theta), -sinf(theta));

			Complex t = W_N_k * (*pingpong[src])[i + j + mh];
			Complex u = (*pingpong[src])[i + j];

			(*pingpong[1 - src])[l] = u + t;
		}

		src = 1 - src;
	}

	assert(src == 0);
#else
	// original iterative algorithm (from wikipedia)
	for( int s = 1; s <= log2_N; ++s ) {
		int m = 1 << s;		// butterfly group height
		int mh = m >> 1;	// butterfly group half height

		// first m-th root of unity
		float theta = GL_2PI / m;
		Complex W_m(cosf(theta), -sinf(theta));

		// for each butterfly group
		for( int l = 0; l < N; l += m ) {
			// initial twiddle factor
			Complex W_m_k(1, 0);

			// for each butterfly in this group
			for( int k = 0; k < mh; ++k ) {
				Complex u = X_fft[l + k];
				Complex t = W_m_k * X_fft[l + k + mh];

				// butterfly operation
				X_fft[l + k] = u + t;
				X_fft[l + k + mh] = u - t;

				// increase twiddle exponent
				W_m_k = W_m_k * W_m;
			}
		}
	}
#endif

	// test
	char buff[64];

	printf("       DFT       vs       FFT       \n------------------------------------\n");

	for( int k = 0; k < N; ++k ) {
		sprintf_s(buff, "(%.3f, %.3f)", X_dft[k].a, X_dft[k].b);

		size_t len = strlen(buff);
		size_t start = 19;

		_strnset_s(buff, ' ', start - len);
		buff[start - len] = 0;

		printf("(%.3f, %.3f)%s(%.3f, %.3f)\n", X_dft[k].a, X_dft[k].b, buff, X_fft[k].a, X_fft[k].b);
	}

#if 0
	printf("\nspectrum:\n");

	for( int k = 0; k < N; ++k ) {
		X_dft[k].a /= N;
		X_dft[k].b /= N;

		float ampl = sqrtf(X_dft[k].a * X_dft[k].a + X_dft[k].b * X_dft[k].b);
		float phase = atan2f(X_dft[k].b, X_dft[k].a);

		printf("(%.3f, %.3f)\n", ampl, phase);
	}
#endif
}
