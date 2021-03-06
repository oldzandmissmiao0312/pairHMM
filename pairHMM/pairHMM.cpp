// pairHMM.cpp: 定义控制台应用程序的入口点。
//
#define _CRT_SECURE_NO_WARNINGS

//#include "stdafx.h"
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdint.h>
#include <math.h>
#include <emmintrin.h>
#include <Windows.h>
#include <vector>
#include "aligned_allocator.h"
using namespace std;
#define BILLION                             (1E9)
//struct timespec { long tv_sec; long tv_nsec; };
static BOOL g_first_time = 1;
static LARGE_INTEGER g_counts_per_sec;

int clock_gettime(int dummy, struct timespec *ct)
{
	LARGE_INTEGER count;

	if (g_first_time)
	{
		g_first_time = 0;

		if (0 == QueryPerformanceFrequency(&g_counts_per_sec))
		{
			g_counts_per_sec.QuadPart = 0;
		}
	}

	if ((NULL == ct) || (g_counts_per_sec.QuadPart <= 0) ||
		(0 == QueryPerformanceCounter(&count)))
	{
		return -1;
	}

	ct->tv_sec = count.QuadPart / g_counts_per_sec.QuadPart;
	ct->tv_nsec = ((count.QuadPart % g_counts_per_sec.QuadPart) * BILLION) / g_counts_per_sec.QuadPart;

	return 0;
}


struct NUM_ADD
{
	short read_number;
	short haplotype_number;
	int address_array;
};
//比对一下时间
double diff(timespec start, timespec end)
{
	double a = 0;
	if ((end.tv_nsec - start.tv_sec)<0)
	{
		a = end.tv_sec - start.tv_sec - 1;
		a += (1000000000 + end.tv_nsec - start.tv_nsec) / 1000000000.0;
	}
	else
	{
		a = end.tv_sec - start.tv_sec + (end.tv_nsec - start.tv_nsec) / 1000000000.0;

	}
	return a;

}

struct InputData
{
	int read_size;
	char read_base[501];
	char base_quals[501];
	char ins_quals[501];
	char del_quals[501];
	char gcp_quals[501];
	int haplotype_size;
	char haplotype_base[501];
};
struct Diagonals {
	vector <float> m;
	vector <float> mp;  //这个Mp既能够表示左边，又能够表示上边,是按照行来存储a
	vector <float> mpp;//m的左上
	vector <float> x;
	vector <float> xp;//代表x的上面
	vector <float> xpp;//x的左上
	vector <float> y;
	vector <float> yp;//代表y的左边
	vector <float> ypp;//y的左上

	Diagonals(const size_t read_length) :
		m(read_length, static_cast<float>(0)),
		mp(read_length, static_cast<float>(0)),
		mpp(read_length, static_cast<float>(0)),
		x(read_length, static_cast<float>(0)),
		xp(read_length, static_cast<float>(0)),
		xpp(read_length, static_cast<float>(0)),
		y(read_length, static_cast<float>(0)),
		yp(read_length, static_cast<float>(0)),
		ypp(read_length, static_cast<float>(0))
	{}
	void rotate() {
		std::swap(mpp, mp);
		std::swap(xpp, xp);
		std::swap(ypp, yp);
		std::swap(mp, m);
		std::swap(xp, x);
		std::swap(yp, y);
	}

	
};
struct Constants
{
	float mm[350];
	float mx[350];
	float my[350];
	float gm[350];
	float xx[350];
	float yy[350];
};

float ph2pr[128];

float  compute_full_prob(InputData * tc)
{
	int r, c;
	int ROWS = tc->read_size;
	int COLS = tc->haplotype_size;
	struct timespec start, finish;
	double  computation_time = 0;

	float  M[350][350];
	float  X[350][350];
	float  Y[350][350];
	float  p[350][6];
	int MM = 0, GapM = 1, MX = 2, XX = 3, MY = 4, YY = 5;
	p[0][MM] = 0;
	p[0][GapM] = 0;
	p[0][MX] = 0;
	p[0][XX] = 0;
	p[0][MY] = 0;
	p[0][YY] = 0;
	for (r = 1; r <= ROWS; r++)
	{
		int _i = tc->ins_quals[r - 1] & 127;
		int _d = tc->del_quals[r - 1] & 127;
		int _c = tc->gcp_quals[r - 1] & 127;


		p[r][MM] = 1.0f - ph2pr[(_i + _d) & 127];


		p[r][GapM] = 0.9;
		p[r][MX] = ph2pr[_i];
		p[r][XX] = 0.1;
		p[r][MY] = ph2pr[_d];
		p[r][YY] = 0.1;

	}
	M[0][0] = 1;
	X[0][0] = 0;
	Y[0][0] = 0;
	for (c = 1; c <= COLS; c++)
	{
		M[0][c] = 0;
		X[0][c] = 0;//不知赋值为0是否正确
		Y[0][c] = (ldexpf(1.f, 120)) / (float)(tc->haplotype_size);//这个是用来防止数值下溢
	}

	for (r = 1; r <= ROWS; r++)
	{
		M[r][0] = 0;
		X[r][0] = X[r - 1][0] * p[r - 1][XX]; //这个初始化操作有意义吗？
		Y[r][0] = 0;
	}

	clock_gettime(0, &start);
	//算法的主要部分
	int cycle = 0;
	struct timespec startc, finishc;
	for (r = 1; r <= ROWS; r++)
		for (c = 1; c <= COLS; c++)
		{
			clock_gettime(0, &startc);
			char _rs = tc->read_base[r - 1];
			char _hap = tc->haplotype_base[c - 1];
			int _q = tc->base_quals[r - 1] & 127;
			float  distm = ph2pr[_q];
			if (_rs == _hap || _rs == 'N' || _hap == 'N')
				distm = 1 - distm;  //if match
			else
				distm = distm / 3; // doesn't match

			//float distm = 1;
			M[r][c] = distm * (M[r - 1][c - 1] * p[r][MM] + X[r - 1][c - 1] * p[r][GapM] + Y[r - 1][c - 1] * p[r][GapM]);

			X[r][c] = M[r - 1][c] * p[r][MX] + X[r - 1][c] * p[r][XX];

			Y[r][c] = M[r][c - 1] * p[r][MY] + Y[r][c - 1] * p[r][YY];
			cycle++;
			clock_gettime(0, &finishc);
			computation_time = diff(startc, finishc);
			//printf("一次运算的时间:%e\n", computation_time);
			
			
				
		}

	//这一块可以加速
	float  result = 0;
	for (c = 1; c <= COLS; c++)
	{
		result += M[ROWS][c] + X[ROWS][c] + Y[ROWS][c];
	} 
	clock_gettime(0, &finish);
	computation_time = diff(start, finish);
	std::cout<<"computation_time1:"<<computation_time<<endl;
	std::cout<<"result1:"<<result<<endl;
	std::cout << cycle << endl;
	return result;
}
float compute_full_prob_sse(InputData *tc)
{
	
	int ROWS = tc->read_size;
	int COLS = tc->haplotype_size;
	struct timespec start, finish;
	double  computation_time = 0;
	
	float  p[350][6];

	
	Constants *consts = (Constants*)malloc(sizeof(Constants));
	
	float m[350];
	float mp[350];
	float mpp[350];
	float x[350];
	float xp[350];
	float xpp[350];
	float y[350];
	float yp[350];
	float ypp[350];
	

	const __m128i N = _mm_castps_si128(_mm_set1_ps('N'));
	const __m128 one = _mm_set1_ps(1.f);
	auto result = 0.f;
	int MM = 0, GapM = 1, MX = 2, XX = 3, MY = 4, YY = 5;
	p[0][MM] = 0;
	p[0][GapM] = 0;
	p[0][MX] = 0;
	p[0][XX] = 0;
	p[0][MY] = 0;
	p[0][YY] = 0;

	for (int r = 1; r <= ROWS; r++)
	{
		int _i = tc->ins_quals[r - 1] & 127;
		int _d = tc->del_quals[r - 1] & 127;
		int _c = tc->gcp_quals[r - 1] & 127;


		consts->mm[r] = 1.0f - ph2pr[(_i + _d) & 127];
		consts->gm[r] = 0.9;
		consts->mx[r] = ph2pr[_i];
		consts->xx[r] = 0.1;
		consts->my[r] = ph2pr[_d];
		consts->yy[r] = 0.1;



	}
	
	

	x[0] = 0;
	xp[0] = 0;
	xpp[0] = 0;
	m[0] = 0;
	mp[0] = 0;
	mpp[0] = 1;
	y[0] = (ldexpf(1.f, 120)) / (tc->haplotype_size);
	yp[0] = (ldexpf(1.f, 120)) / (tc->haplotype_size);
	ypp[0] = 0;


	
	for (int r = 1; r <= ROWS; r++)
	{
		//diags.m[r] = 0;
		//diags.mp[r] = 0;
		//diags.mpp[r] = 0;


		//diags.x[r] = 0;
		//diags.xp[r] = 0;
		//diags.xpp[r] = 0;

		m[r] = 0;
		mp[r] = 0;
		mpp[r] = 0;


		x[r] = 0;
		xp[r] = 0;
		xpp[r] = 0;

		y[r] = 0;
		yp[r] = 0;
		ypp[r] = 0;

	}

	auto hap_last = COLS + ROWS - 1;
	int cycle = 0;
	struct timespec startc, finishc;
	clock_gettime(0, &start);
	for (int d = 1; d <= hap_last; d++) {
		if (d <= COLS)
		{
			for (int r = 1u; r <= min(ROWS, d); r += 4)
			{
				clock_gettime(0, &startc);
				//判断一下条件
				
				__m128i read_base = _mm_castps_si128(
					_mm_set_ps(
						tc->read_base[r + 2],
						tc->read_base[r + 1],
						tc->read_base[r],
						tc->read_base[r - 1]));
				__m128i hap_base = _mm_castps_si128(
					_mm_set_ps(
						tc->haplotype_base[max(0, d - r - 3)],
						tc->haplotype_base[max(0, d - r - 2)],
						tc->haplotype_base[max(0, d - r - 1)],
						tc->haplotype_base[max(0, d - r)]));

				


				__m128i _q = _mm_set_epi32(
					tc->base_quals[min(ROWS-1,r + 2)],
					tc->base_quals[min(ROWS-1,r + 1)],
					tc->base_quals[min(ROWS-1,r)],
					tc->base_quals[min(ROWS-1,r - 1)]);

				
				int s1 = 0;
				int s2 = 0;
				int s3 = 0;
				int s4 = 0;
				s1 = _mm_extract_epi16(_q, 0);
				s2 = _mm_extract_epi16(_q, 2);
				s3 = _mm_extract_epi16(_q, 4);
				s4 = _mm_extract_epi16(_q, 6);
				
				__m128 distm = _mm_set_ps(ph2pr[s4], ph2pr[s3], ph2pr[s2], ph2pr[s1]);
				


				__m128 one_minus_base_qual = _mm_sub_ps(one, distm);
				__m128i cmp_r_h = _mm_cmpeq_epi32(read_base, hap_base);
				__m128i cmp_r_n = _mm_cmpeq_epi32(read_base, N);
				__m128i cmp_h_n = _mm_cmpeq_epi32(hap_base, N);

				__m128i cmp = _mm_or_si128(cmp_r_h,
					_mm_or_si128(cmp_r_n, cmp_h_n));

				distm = _mm_div_ps(distm, _mm_set1_ps(3.f));
				
				__m128 prior = _mm_castsi128_ps(
					_mm_or_si128(
						_mm_and_si128(cmp, _mm_castps_si128(one_minus_base_qual)),
						_mm_andnot_si128(cmp, _mm_castps_si128(distm))));


				
				_mm_store_ps(&m[r], _mm_mul_ps(prior,
					_mm_add_ps(_mm_mul_ps(_mm_loadu_ps(&mpp[r - 1]), _mm_loadu_ps(&consts->mm[r])),
						_mm_mul_ps(_mm_loadu_ps(&consts->gm[r]),
							_mm_add_ps(_mm_loadu_ps(&xpp[r - 1]), _mm_loadu_ps(&ypp[r - 1]))))));

							

				_mm_store_ps(&x[r], _mm_add_ps(
					_mm_mul_ps(_mm_loadu_ps(&mp[r - 1]), _mm_loadu_ps(&consts->mx[r])),
					_mm_mul_ps(_mm_loadu_ps(&xp[r - 1]), _mm_loadu_ps(&consts->xx[r]))));

				_mm_store_ps(&y[r], _mm_add_ps(
					_mm_mul_ps(_mm_loadu_ps(&mp[r]), _mm_loadu_ps(&consts->my[r])),
					_mm_mul_ps(_mm_loadu_ps(&yp[r]), _mm_loadu_ps(&consts->yy[r]))));
				clock_gettime(0, &finishc);
				computation_time = diff(startc, finishc);
				//printf("sse一次运算的时间%e\n", computation_time);
				cycle++;

			}
		}
		else 
		{
			for (int r = d - COLS + 1; r <= min(ROWS,d); r += 4)
			{
				//判断一下条件

				__m128i read_base = _mm_castps_si128(
					_mm_set_ps(
						tc->read_base[r + 2],
						tc->read_base[r + 1],
						tc->read_base[r],
						tc->read_base[r - 1]));
				__m128i hap_base = _mm_castps_si128(
					_mm_set_ps(
						tc->haplotype_base[max(0, d - r - 3)],
						tc->haplotype_base[max(0, d - r - 2)],
						tc->haplotype_base[max(0, d - r - 1)],
						tc->haplotype_base[max(0, d - r)]));



				
				__m128i _q = _mm_set_epi32(
					tc->base_quals[min(ROWS - 1,r + 2)],
					tc->base_quals[min(ROWS - 1,r + 1)],
					tc->base_quals[min(ROWS - 1,r)],
					tc->base_quals[min(ROWS - 1,r - 1)]);
				
				int s1 = 0;
				int s2 = 0;
				int s3 = 0;
				int s4 = 0;
				s1 = _mm_extract_epi16(_q, 0);
				s2 = _mm_extract_epi16(_q, 2);
				s3 = _mm_extract_epi16(_q, 4);
				s4 = _mm_extract_epi16(_q, 6);

				__m128 distm = _mm_set_ps(ph2pr[s4], ph2pr[s3], ph2pr[s2], ph2pr[s1]);



				__m128 one_minus_base_qual = _mm_sub_ps(one, distm);
				__m128i cmp_r_h = _mm_cmpeq_epi32(read_base, hap_base);
				__m128i cmp_r_n = _mm_cmpeq_epi32(read_base, N);
				__m128i cmp_h_n = _mm_cmpeq_epi32(hap_base, N);

				__m128i cmp = _mm_or_si128(cmp_r_h,
					_mm_or_si128(cmp_r_n, cmp_h_n));

				distm = _mm_div_ps(distm, _mm_set1_ps(3.f));

				__m128 prior = _mm_castsi128_ps(
					_mm_or_si128(
						_mm_and_si128(cmp, _mm_castps_si128(one_minus_base_qual)),
						_mm_andnot_si128(cmp, _mm_castps_si128(distm))));

				
				_mm_store_ps(&m[r], _mm_mul_ps(prior,
					_mm_add_ps(_mm_mul_ps(_mm_loadu_ps(&mpp[r - 1]), _mm_loadu_ps(&consts->mm[r])),
						_mm_mul_ps(_mm_loadu_ps(&consts->gm[r]),
							_mm_add_ps(_mm_loadu_ps(&xpp[r - 1]), _mm_loadu_ps(&ypp[r - 1]))))));

				_mm_store_ps(&x[r], _mm_add_ps(
					_mm_mul_ps(_mm_loadu_ps(&mp[r - 1]), _mm_loadu_ps(&consts->mx[r])),
					_mm_mul_ps(_mm_loadu_ps(&xp[r - 1]), _mm_loadu_ps(&consts->xx[r]))));

				_mm_store_ps(&y[r], _mm_add_ps(
					_mm_mul_ps(_mm_loadu_ps(&mp[r]), _mm_loadu_ps(&consts->my[r])),
					_mm_mul_ps(_mm_loadu_ps(&yp[r]), _mm_loadu_ps(&consts->yy[r]))));
				cycle++;
			}
		}
		
		result += m[ROWS] + x[ROWS] + +y[ROWS];
		for (int j = 0; j <= ROWS; j++)
		{
			mpp[j] = mp[j];
			mp[j] = m[j];

			xpp[j] = xp[j];
			xp[j] = x[j];

			ypp[j] = yp[j];
			yp[j] = y[j];
		}
		
		
	}



	
	
	clock_gettime(0, &finish);
	computation_time = diff(start, finish);
	std::cout<<"computation_time2:"<<computation_time<<endl;
	std::cout<<"result2:"<<result<<endl;
	std::cout << cycle << endl;
	return result;
	
	
}


#define CLOCK_MONOTONIC_RAW 0
int main(int argc, char * argv[])
{
	//这里对转移概率进行了初始化
	for (int i = 0; i<128; i++)
	{
		ph2pr[i] = powf(10.f, -((float)i) / 10.f);
	}

	struct timespec start, finish;
	double  computation_time_1 = 0, computation_time_2 = 0, mem_cpy_time = 0, read_time = 0, data_prepare = 0;
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);

	FILE * file;
	// file=fopen(argv[1],"r");
	file = fopen("E:\\rencentwork\\0225\\pairHMM_c++_without_avx\\128_data.txt", "r");
	//file = fopen("E:\\最近工作\\PairHMM\\pairHMM_c++_without_avx\\160_data.txt", "r");
	int size;
	fscanf(file, "%d", &size);
	std::cout<<size<<endl;
	int total = 0;
	int fakesize = 0;
	InputData *inputdata = 0;
	while (!feof(file))
	{
		total += size;
		char useless;
		useless = fgetc(file);

		fakesize = 1000;
		inputdata = (InputData*)malloc(fakesize*(sizeof(InputData)));
		for (int i = 0; i<size; i++)
		{
			int read_size;
			//read;
			fscanf(file, "%d\n", &inputdata[i].read_size);
			fscanf(file, "%s ", inputdata[i].read_base);
			read_size = inputdata[i].read_size;
			//先读base_quals the level of confdence in the correctness of each symbol in each read sequence, which we represent as  Q_base
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				fscanf(file, "%d ", &aa);
				//		 printf("quals=%d ",aa);
				inputdata[i].base_quals[j] = (char)aa;
			}
			//base insertion quality Q_i
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				fscanf(file, "%d ", &aa);
				//		 printf("ins= %d ",aa);
				inputdata[i].ins_quals[j] = (char)aa;
			}
			//base deletion quality Q_d
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				fscanf(file, "%d ", &aa);
				//		 printf("del= %d ",aa);
				inputdata[i].del_quals[j] = (char)aa;
			}
			//接着读gap continuation penalty Q_g
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				if (j<size - 1) fscanf(file, "%d ", &aa);
				else  fscanf(file, "%d \n", &aa);
				//		 printf("gcp= %d ",aa);
				inputdata[i].gcp_quals[j] = (char)aa;
			}
			//		printf("\n");
			//haplotyp
			int haplotype_size;
			fscanf(file, "%d\n", &inputdata[i].haplotype_size);
			//注意这里的haplotype_base是字符串
			fscanf(file, "%s\n", inputdata[i].haplotype_base);

		}
		clock_gettime(CLOCK_MONOTONIC_RAW, &finish);
		read_time += diff(start, finish);
	}
	///c++ begin!!!!!
	//
	//size = fakesize;
	for (int i = 1; i<fakesize; i++)
	{
		//      printf("%d ",i);
		inputdata[i].read_size = inputdata[0].read_size;
		memcpy(inputdata[i].read_base, inputdata[0].read_base, inputdata[0].read_size);
		for (int j = 0; j<inputdata[0].read_size; j++)
		{
			inputdata[i].base_quals[j] = inputdata[0].base_quals[j];
			inputdata[i].ins_quals[j] = inputdata[0].ins_quals[j];
			inputdata[i].del_quals[j] = inputdata[0].del_quals[j];
			inputdata[i].gcp_quals[j] = inputdata[0].gcp_quals[j];
		}
		inputdata[i].haplotype_size = inputdata[0].haplotype_size;
		memcpy(inputdata[i].haplotype_base, inputdata[0].haplotype_base, inputdata[0].haplotype_size);
	}


	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	float * result1;
	result1 = (float *)malloc(sizeof(float)*size);
	for (int i = 0; i<size; i++)
	{
		result1[i] = compute_full_prob(&inputdata[i]);
	}
	clock_gettime(CLOCK_MONOTONIC_RAW, &finish);
	computation_time_1 += diff(start, finish);
	clock_gettime(CLOCK_MONOTONIC_RAW, &start);
	//cout << "code1\nread_time=" << read_time << "\ndata_prepare=" << data_prepare << "\ncomputation_time=" << computation_time_1 << "\nmemory_copy_time=" << mem_cpy_time << endl;


	float * result2;
	result2 = (float *)malloc(sizeof(float)*size);
	
	for (int i = 0; i<size; i++)
	{
		result2[i] = compute_full_prob_sse(&inputdata[i]);
	}

	clock_gettime(CLOCK_MONOTONIC_RAW, &finish);
	computation_time_2 += diff(start, finish);


	

	free(result1);
	free(result2);
	free(inputdata);
	fscanf(file, "%d", &size);





	//cout << "code2\nread_time="<<read_time<<"\ndata_prepare="<< data_prepare <<"\ncomputation_time="<<computation_time_2<<"\nmemory_copy_time="<< mem_cpy_time <<endl;
	system("pause");
	return 0;
}