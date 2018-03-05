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
#include <nmmintrin.h>
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
	vector <float> m[350];
	vector <float> mp[350];  //这个Mp既能够表示左边，又能够表示上边,是按照行来存储a
	vector <float> mpp[350];//m的左上
	vector <float> x[350];
	vector <float> xp[350];//代表x的上面
	vector <float> xpp[350];//x的左上
	vector <float> y[350];
	vector <float> yp[350];//代表y的左边
	vector <float> ypp[350];//y的左上

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

	for (c = 0; c <= COLS; c++)
	{
		M[0][c] = 0;
		X[0][c] = 0;
		Y[0][c] = (ldexpf(1.f, 120)) / (float)(tc->haplotype_size);
	}

	for (r = 1; r <= ROWS; r++)
	{
		M[r][0] = 0;
		X[r][0] = X[r - 1][0] * p[r - 1][XX];
		Y[r][0] = 0;
	}
	M[0][0] = 1;
	clock_gettime(0, &start);
	//算法的主要部分
	for (r = 1; r <= ROWS; r++)
		for (c = 1; c <= COLS; c++)
		{
			/*
			char _rs = tc->read_base[r - 1];
			char _hap = tc->haplotype_base[c - 1];
			int _q = tc->base_quals[r - 1] & 127;
			float  distm = ph2pr[_q];
			if (_rs == _hap || _rs == 'N' || _hap == 'N')
				distm = 1 - distm;
			else
				distm = distm / 3;
*/
			float distm = 1;
			M[r][c] = distm * (M[r - 1][c - 1] * p[r][MM] + X[r - 1][c - 1] * p[r][GapM] + Y[r - 1][c - 1] * p[r][GapM]);

			X[r][c] = M[r - 1][c] * p[r][MX] + X[r - 1][c] * p[r][XX];

			Y[r][c] = M[r][c - 1] * p[r][MY] + Y[r][c - 1] * p[r][YY];

		}

	//这一块可以加速
	float  result = 0;
	for (c = 1; c <= COLS; c++)
	{
		result += M[ROWS][c] + X[ROWS][c];
	}
	clock_gettime(0, &finish);
	computation_time = diff(start, finish);
	printf("computation_time1:%e\n", computation_time);
	printf("result1:%e\n", result);
	return result;
}
float compute_full_prob_sse(InputData *tc)
{
	int r, c;
	int ROWS = tc->read_size;
	int COLS = tc->haplotype_size;
	struct timespec start, finish;
	double  computation_time = 0;
	
	float  p[350][6];

	Diagonals *diags = (Diagonals*)malloc(sizeof(Diagonals));
	Constants *consts = (Constants*)malloc(sizeof(Constants));
	
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

	for (r = 1; r <= ROWS; r++)
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

	diags->x[0] = 0;
	diags->xp[0] = 0;
	diags->xpp[0] = 0;
	diags->m[0] = 0;
	diags->mp[0] = 0;
	diags->mpp[0] = 1;
	diags->y[0] = (ldexpf(1.f, 120)) / (tc->haplotype_size);
	diags->yp[0] = (ldexpf(1.f, 120)) / (tc->haplotype_size);
	diags->ypp[0] = (ldexpf(1.f, 120)) / (tc->haplotype_size);


	
	for (r = 1; r <= ROWS; r++)
	{
		diags->m[r] = 0;
		diags->mp[r] = 0;
		diags->mpp[r] = 0;


		diags->x[r] = 0;
		diags->xp[r] = 0;
		diags->xpp[r] = 0;

	}

	auto hap_last = COLS + ROWS + 1;
	clock_gettime(0, &start);
	for (int d = 1; d != hap_last; ++d) {
		for (int r = 1u; r < ROWS; r += 4)
		{
			 //判断一下条件
			
			__m128i read_base = _mm_castps_si128(_mm_set_ps(tc->read_base[r],tc->read_base[r+1],tc->read_base[r+2],tc->read_base[r+3]));// _mm_castps_si128(_mm_loadu_ps(&(tc->read_base[r])));
			__m128i hap_base = _mm_castps_si128(_mm_set_ps(tc->haplotype_base[hap_last + r - d], tc->haplotype_base[hap_last + r - d+1], tc->haplotype_base[hap_last + r - d+2], tc->haplotype_base[hap_last + r - d+3]));
			__m128 base_qual = _mm_set_ps(tc->base_quals[r], tc->base_quals[r+1], tc->base_quals[r+2], tc->base_quals[r+3]);
			__m128 one_minus_base_qual = _mm_sub_ps(one, base_qual);
			__m128i cmp = _mm_or_si128(_mm_cmpeq_epi32(read_base, hap_base),
				_mm_or_si128(_mm_cmpeq_epi32(read_base, N), _mm_cmpeq_epi32(hap_base, N)));

			base_qual = _mm_div_ps(base_qual, _mm_set1_ps(3.f));
			__m128 prior = _mm_castsi128_ps(
				_mm_or_si128(
					_mm_and_si128(cmp, _mm_castps_si128(one_minus_base_qual)),
					_mm_andnot_si128(cmp, _mm_castps_si128(base_qual))));
			
			//__m128 prior = _mm_set1_ps(1);
			_mm_store_ps(&diags->m[r], _mm_mul_ps(prior,
				_mm_add_ps(_mm_mul_ps(_mm_loadu_ps(&diags->mpp[r - 1]), _mm_loadu_ps(&consts->mm[r])),
					_mm_mul_ps(_mm_loadu_ps(&consts->gm[r]),
						_mm_add_ps(_mm_loadu_ps(&(diags->xpp[r - 1])), _mm_loadu_ps(&(diags->ypp[r - 1])))))));

			_mm_store_ps(&diags->x[r], _mm_add_ps(
				_mm_mul_ps(_mm_loadu_ps(&diags->mp[r - 1]), _mm_loadu_ps(&consts->mx[r])),
				_mm_mul_ps(_mm_loadu_ps(&diags->xp[r - 1]), _mm_loadu_ps(&consts->xx[r]))));

			_mm_store_ps(&diags->y[r], _mm_add_ps(
				_mm_mul_ps(_mm_loadu_ps(&diags->mp[r]), _mm_loadu_ps(&consts->my[r])),
				_mm_mul_ps(_mm_loadu_ps(&diags->yp[r]), _mm_loadu_ps(&consts->yy[r]))));

		}
		
		result += diags->m[ROWS - 1] + diags->x[ROWS - 1];
		diags->rotate(); //这个操作很费时间,说明还是设计影响到了速度
		//printf("%e", result);
	}




	
	clock_gettime(0, &finish);
	computation_time = diff(start, finish);
	printf("computation_time2:%e\n", computation_time);
	printf("result2:%e\n", result);
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
	//file = fopen("E:\\rencentwork\\0225\\pairHMM_c++_without_avx\\256_data.txt", "r");
	file = fopen("E:\\最近工作\\PairHMM\\pairHMM_c++_without_avx\\256_data.txt", "r");
	int size;
	fscanf(file, "%d", &size);
	printf("%d\n", size);
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
			//先读base_quals
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				fscanf(file, "%d ", &aa);
				//		 printf("quals=%d ",aa);
				inputdata[i].base_quals[j] = (char)aa;
			}
			//接着读ins_quals
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				fscanf(file, "%d ", &aa);
				//		 printf("ins= %d ",aa);
				inputdata[i].ins_quals[j] = (char)aa;
			}
			//接着读del_quals
			for (int j = 0; j<read_size; j++)
			{
				int  aa;
				fscanf(file, "%d ", &aa);
				//		 printf("del= %d ",aa);
				inputdata[i].del_quals[j] = (char)aa;
			}
			//接着读gcp_quals
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
	printf("code1\nread_time=%e\n  data_prepare=%e\n  computation_time=%e\n memory_copy_time=%e\n total_time=%e\n", read_time, data_prepare, computation_time_1, mem_cpy_time, computation_time_1 + mem_cpy_time + data_prepare);

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





	printf("code2\nread_time=%e\n  data_prepare=%e\n  computation_time=%e\n memory_copy_time=%e\n total_time=%e\n", read_time, data_prepare, computation_time_2, mem_cpy_time, computation_time_2 + mem_cpy_time + data_prepare);
	system("pause");
	return 0;
}