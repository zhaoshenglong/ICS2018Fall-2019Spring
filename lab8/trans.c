/*
 * Information For Lab8:
 *  StudentID: ics515030910241
 *  StudentName: zhaoshenglong 
 * 
 */
/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);
void trans_32(int M, int N, int A[N][M], int B[M][N]);
void trans_64(int M, int N, int A[N][M], int B[M][N]);
void trans_61_67(int M, int N, int A[N][M], int B[M][N]);
void trans(int M, int N, int A[N][M], int B[M][N]);
/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if ((M == N) && (M == 32))
        trans_32(M, N, A, B);
    else if ((M == N) && (M == 64))
        trans_64(M, N, A, B);
    else if ((M == 61) && (N == 67))
        trans_61_67(M, N, A, B);
    else
        trans(M, N, A, B);
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */

/*
 * trans_32 - A special transpose for transpose 32 x 32 matrix
 *          Local variable: bi, bj, i, j, bb
 *          Total 5
 *          bi - block row 8 X 8
 *          bj - block collomn 8 X 8
 *          i  - index row
 *          j  - index collomn
 *          bb - buffer block 1 X 1
 */
char trans_32_desc[] = "Simple blocked transpose";
void trans_32(int M, int N, int A[N][M], int B[M][N])
{
    int bb; /* Temp variable for diagonal element*/
    for (int bi = 0; bi < M; bi += 8)
    {
        for (int bj = 0; bj < N; bj += 8)
        {
            for (int i = bi; i < bi + 8; i++)
            {
                for (int j = bj; j < bj + 8; j++)
                {
                    /* If is diagonal, conflict miss */
                    if ((i - bi) == (j - bj))
                    {
                        bb = A[i][j];
                    }
                    else
                        B[j][i] = A[i][j];
                }
                B[bj + i - bi][i] = bb;
            }
        }
    }
}
/*
 * trans_64 - Transpose for 64 x 64 Matrix
 *          Local variable: bi, bj, i, j, bb
 *          Total 5
 *          bi - block row 8 X 8
 *          bj - block collomn 8 X 8
 *          i  - index row
 *          j  - index collomn
 *          bb - buffer block start  4 X 8
 *  Methods: First, transpose the diagonal block (8 X 8)
 *           Second, transpose the non diagonal block ,use B[56][56] as a buffer
 *  This step, only transpose B[0][0] ~ B[48][48], remained the last row and collomn
 *           Third, transpose the last row and collom ,use B[56][48] as a buffer
 *  This, step, remained the B[48][56], B[56][56] and B[56][48]
 *           Fourth, transpose the B[56][56]
 *           Last, transpose the B[48][56] and B[56][48]     
 */
char trans_64_desc[] = "Blocked transpose for 64 x 64";
void trans_64(int M, int N, int A[N][M], int B[M][N])
{
    int bi, bj, i, j, bb;
    /* Deal with the diagonal case first */
    /* First, transpose the first 1 diagonal block( 8 x 8) */
    for (bi = 0; bi < 8; bi += 4)
    {
        for (i = bi; i < 4 + bi; i++)
        {
            for (j = 0; j < 8; j++)
            {
                /* Copy into free block in B B[0][8] ~ B[7][15] */
                B[i - bi][j + 8] = A[i][j];
            }
        }
        /* Transpose in B */
        if (bi == 0)
        {
            for (bj = 0; bj < 8; bj += 4)
            {
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 4; j++)
                    {
                        B[j + bj][i + bi] = B[i][j + 8 + bj];
                    }
                }
            }
        }
        /* Reverse order to avoid conflict miss */
        else
        {
            for (bj = 4; bj >= 0; bj -= 4)
            {
                for (i = 3; i >= 0; i--)
                {
                    for (j = 3; j >= 0; j--)
                    {
                        B[j + bj][i + bi] = B[i][j + 8 + bj];
                    }
                }
            }
        }
    }

    /* Then, deal with the rest diagonal block, use B[8][0] ~ B[11][7] */
    /* Left the last block for latter buffer block */
    for (bj = 8; bj < 56; bj += 8)
    {
        for (bi = bj; bi < 8 + bj; bi += 4)
        {
            for (i = bi; i < bi + 4; i++)
            {
                for (j = bj; j < bj + 8; j++)
                {
                    /* Copy in to B[8][0] ~ B[15][7] */
                    B[i - bi + 8][j - bj] = A[i][j];
                }
            }
            /* Transpose the block */
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    B[j + bj][i + bi] = B[i + 8][j];
                }
            }
            for (i = 0; i < 4; i++)
            {
                for (j = 0; j < 4; j++)
                {
                    B[j + bj + 4][i + bi] = B[i + 8][j + 4];
                }
            }
        }
    }

    /* Transpose the non diagonal block, 56 x 56 */
    bb = 56;
    for (bi = 0; bi < 56; bi += 8)
    {
        for (bj = 0; bj < 56; bj += 8)
        {
            /* Spare the last three triangle block for buffer */
            if (bi != bj)
            {
                /* Copy into the buffer block */
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 8; j++)
                    {
                        B[56 + i][j + bb] = A[bi + i][bj + j];
                    }
                }
                /* Move to the distination */
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 4; j++)
                    {
                        B[bj + j][bi + i] = B[56 + i][bb + j];
                        B[bj + j][bi + i + 4] = A[bi + i + 4][bj + j];
                    }
                }
                for (i = 0; i < 4; i++)
                {
                    for (j = 4; j < 8; j++)
                    {
                        B[bj + j][bi + i] = B[56 + i][bb + j];
                        B[bj + j][bi + i + 4] = A[bi + i + 4][bj + j];
                    }
                }
            }
        }
    }
    /* Transpose the last collomn and row */
    bb = 48;
    for (bi = 0; bi < 48; bi += 8)
    {
        for (bj = 56; bj < 64; bj += 8)
        {
            /* Spare the last three triangle block for buffer */
            if (bi != bj)
            {
                /* Copy into the buffer block */
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 8; j++)
                    {
                        B[56 + i][j + bb] = A[bi + i][bj + j];
                    }
                }
                /* Move to the distination */
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 4; j++)
                    {
                        B[bj + j][bi + i] = B[56 + i][bb + j];
                        B[bj + j][bi + i + 4] = A[bi + i + 4][bj + j];
                    }
                }
                for (i = 0; i < 4; i++)
                {
                    for (j = 4; j < 8; j++)
                    {
                        B[bj + j][bi + i] = B[56 + i][bb + j];
                        B[bj + j][bi + i + 4] = A[bi + i + 4][bj + j];
                    }
                }
            }
        }
    }
    for (bi = 56; bi < 64; bi += 8)
    {
        for (bj = 0; bj < 48; bj += 8)
        {
            /* Spare the last three triangle block for buffer */
            if (bi != bj)
            {
                /* Copy into the buffer block */
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 8; j++)
                    {
                        B[56 + i][j + bb] = A[bi + i][bj + j];
                    }
                }
                /* Move to the distination */
                for (i = 0; i < 4; i++)
                {
                    for (j = 0; j < 4; j++)
                    {
                        B[bj + j][bi + i] = B[56 + i][bb + j];
                        B[bj + j][bi + i + 4] = A[bi + i + 4][bj + j];
                    }
                }
                for (i = 0; i < 4; i++)
                {
                    for (j = 4; j < 8; j++)
                    {
                        B[bj + j][bi + i] = B[56 + i][bb + j];
                        B[bj + j][bi + i + 4] = A[bi + i + 4][bj + j];
                    }
                }
            }
        }
    }
    /* Copy the last three block */
    for (bi = 56; bi < 64; bi += 4)
    {
        for (i = bi; i < bi + 4; i++)
        {
            for (j = 56; j < 64; j++)
            {
                B[i - bi + 56][j - 8] = A[i][j];
            }
        }
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                B[j + 56][i + bi] = B[i + 56][j + 48];
            }
        }
        for (i = 0; i < 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                B[j + 60][i + bi] = B[i + 56][j + 52];
            }
        }
    }
    for (bi = 48; bi < 56; bi += 4)
    {
        for (i = bi; i < bi + 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                B[j + 56][i] = A[i][j + 56];
            }
        }
        for (i = bi; i < bi + 4; i++)
        {
            for (j = 4; j < 8; j++)
            {
                B[j + 56][i] = A[i][j + 56];
            }
        }
    }
    for (bi = 56; bi < 64; bi += 4)
    {
        for (i = bi; i < bi + 4; i++)
        {
            for (j = 0; j < 4; j++)
            {
                B[j + 48][i] = A[i][j + 48];
            }
        }
        for (i = bi; i < bi + 4; i++)
        {
            for (j = 4; j < 8; j++)
            {
                B[j + 48][i] = A[i][j + 48];
            }
        }
    }
}
/**
 * trans_61_67 - Simple blocked transpose matrix
 *              Local variable: bi, bj, i, j
 *              Total 4
 *      Since it is hard to apply a specific routine 
 *      to solve the matrix, the common way is good enough, for the 
 *      truth that there is little confict miss
 */
char trans_61_67_desc[] = "Simple blocked transpose for 61 x 67 Matrix";
void trans_61_67(int M, int N, int A[N][M], int B[M][N])
{

    int bi, bj, i, j;
    for (bi = 0; bi < N; bi += 16)
    {
        for (bj = 0; bj < M; bj += 16)
        {

            for (i = bi; i < bi + 16 && i < N; i++)
            {
                for (j = bj; j < bj + 16 && j < M; j++)
                {
                    B[j][i] = A[i][j];
                }
            }
        }
    }
}
/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; j++)
        {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++)
    {
        for (j = 0; j < M; ++j)
        {
            if (A[i][j] != B[j][i])
            {
                return 0;
            }
        }
    }
    return 1;
}
