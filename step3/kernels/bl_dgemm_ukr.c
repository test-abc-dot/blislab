#include <bl_config.h>
#include "bl_dgemm_kernel.h"

//micro-panel a is stored in column major, lda=DGEMM_MR.
#define a(i,j) a[ (j)*DGEMM_MR + (i) ]
//micro-panel b is stored in row major, ldb=DGEMM_NR.
#define b(i,j) b[ (i)*DGEMM_NR + (j) ]
//result      c is stored in column major.
#define c(i,j) c[ (j)*ldc + (i) ]

#define inc_t unsigned long long

void bl_dgemm_ukr( int    k,
                   double *a,
                   double *b,
                   double *c,
                   unsigned long long ldc,
                   aux_t* data )
{
//    int l, j, i;
//
//    for ( l = 0; l < k; ++l )
//    {
//        for ( j = 0; j < DGEMM_NR; ++j )
//        {
//            for ( i = 0; i < DGEMM_MR; ++i )
//            {
//                c( i, j ) += a( i, l ) * b( l, j );
//            }
//        }
//    }
    const inc_t cs_c = ldc;
    const inc_t rs_c = 1;
    double alpha_val = 1.0, beta_val = 1.0;
    double *alpha, *beta;

    alpha = &alpha_val;
    beta  = &beta_val;

    ////void* a_next = bli_auxinfo_next_a( data );
    //void* b_next = bli_auxinfo_next_b( data );
    double *b_next = data->b_next;

    dim_t k_iter  = (unsigned long long)k / 2;
    dim_t k_left  = (unsigned long long)k % 2;

    dim_t i;

    double *c00, *c01, *c02, *c03;
    double *c40, *c41, *c42, *c43;

    // Quad registers.
    __m256d va0_3, va4_7;
    __m256d vA0_3, vA4_7;
    __m256d vb0, vb1, vb2, vb3;
    __m256d vb;
    __m256d vB0;

    __m256d va0_3b_0, va4_7b_0;
    __m256d va0_3b_1, va4_7b_1;
    __m256d va0_3b_2, va4_7b_2;
    __m256d va0_3b_3, va4_7b_3;

    __m256d va0_3b0, va4_7b0;
    __m256d va0_3b1, va4_7b1;
    __m256d va0_3b2, va4_7b2;
    __m256d va0_3b3, va4_7b3;


    __m256d valpha, vbeta, vtmp;
    __m256d vc0_3_0, vc0_3_1, vc0_3_2, vc0_3_3;
    __m256d vc4_7_0, vc4_7_1, vc4_7_2, vc4_7_3;

    __m128d aa, bb;


    __asm__ volatile( "prefetcht0 0(%0)          \n\t" : :"r"(a)  );
    __asm__ volatile( "prefetcht2 0(%0)          \n\t" : :"r"(b_next)  );
    __asm__ volatile( "prefetcht0 0(%0)          \n\t" : :"r"(c)  );



    va0_3b0 = _mm256_setzero_pd();
    va0_3b1 = _mm256_setzero_pd();
    va0_3b2 = _mm256_setzero_pd();
    va0_3b3 = _mm256_setzero_pd();

    va4_7b0 = _mm256_setzero_pd();
    va4_7b1 = _mm256_setzero_pd();
    va4_7b2 = _mm256_setzero_pd();
    va4_7b3 = _mm256_setzero_pd();

    va0_3b_0 = _mm256_setzero_pd();
    va0_3b_1 = _mm256_setzero_pd();
    va0_3b_2 = _mm256_setzero_pd();
    va0_3b_3 = _mm256_setzero_pd();

    va4_7b_0 = _mm256_setzero_pd();
    va4_7b_1 = _mm256_setzero_pd();
    va4_7b_2 = _mm256_setzero_pd();
    va4_7b_3 = _mm256_setzero_pd();

    // Load va0_3
    va0_3 = _mm256_load_pd( a );
    // Load va4_7
    va4_7 = _mm256_load_pd( a + 4 );

    // Load vb (b0,b1,b2,b3)
    vb0 = _mm256_load_pd( b );


    for( i = 0; i < k_iter; ++i )
    {
        __asm__ volatile( "prefetcht0 192(%0)          \n\t" : :"r"(a)  );

        // Load va0_3 (Prefetch)
        // Prefetch A03
        vA0_3 = _mm256_load_pd( a + 8 );

        // Iteration 0.
        vtmp = _mm256_mul_pd( va0_3, vb0 );
        va0_3b_0 = _mm256_add_pd( va0_3b_0, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb0 );
        va4_7b_0 = _mm256_add_pd( va4_7b_0, vtmp );

        // Load va4_7 (Prefetch)
        // Prefetch A47
        vA4_7 = _mm256_load_pd( a + 12 );

        // Shuffle vb (b1,b0,b3,b2)
        vb1 = _mm256_shuffle_pd( vb0, vb0, 0x5 );

        vtmp = _mm256_mul_pd( va0_3, vb1 );
        va0_3b_1 = _mm256_add_pd( va0_3b_1, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb1 );
        va4_7b_1 = _mm256_add_pd( va4_7b_1, vtmp );

        // Permute vb (b3,b2,b1,b0)
        vb2 = _mm256_permute2f128_pd( vb1, vb1, 0x1 );

        // Load vb (b0,b1,b2,b3) (Prefetch)
        vB0 = _mm256_load_pd( b + 4 );

        vtmp = _mm256_mul_pd( va0_3, vb2 );
        va0_3b_2 = _mm256_add_pd( va0_3b_2, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb2 );
        va4_7b_2 = _mm256_add_pd( va4_7b_2, vtmp );

        // Shuffle vb (b3,b2,b1,b0)
        vb3 = _mm256_shuffle_pd( vb2, vb2, 0x5 );

        vtmp = _mm256_mul_pd( va0_3, vb3 );
        va0_3b_3 = _mm256_add_pd( va0_3b_3, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb3 );
        va4_7b_3 = _mm256_add_pd( va4_7b_3, vtmp );

        // Iteration 1.

        __asm__ volatile( "prefetcht0 512(%0)          \n\t" : :"r"(a)  );

        // Load va0_3 (Next iteration)
        va0_3 = _mm256_load_pd( a + 16 );

        vtmp = _mm256_mul_pd( vA0_3, vB0 );
        va0_3b_0 = _mm256_add_pd( va0_3b_0, vtmp );

        vb1 = _mm256_shuffle_pd( vB0, vB0, 0x5 );

        vtmp = _mm256_mul_pd( vA4_7, vB0 );
        va4_7b_0 = _mm256_add_pd( va4_7b_0, vtmp );

        vtmp = _mm256_mul_pd( vA0_3, vb1 );
        va0_3b_1 = _mm256_add_pd( va0_3b_1, vtmp );

        // Load va4_7 (Next iteration)
        va4_7 = _mm256_load_pd( a + 20 );

        vb2 = _mm256_permute2f128_pd( vb1, vb1, 0x1 );

        vtmp = _mm256_mul_pd( vA4_7, vb1 );
        va4_7b_1 = _mm256_add_pd( va4_7b_1, vtmp );

        vtmp = _mm256_mul_pd( vA0_3, vb2 );
        va0_3b_2 = _mm256_add_pd( va0_3b_2, vtmp );

        vb3 = _mm256_shuffle_pd( vb2, vb2, 0x5 );

        vtmp = _mm256_mul_pd( vA4_7, vb2 );
        va4_7b_2 = _mm256_add_pd( va4_7b_2, vtmp );

        // Load vb0(Next iteration)
        vb0 = _mm256_load_pd( b + 8 );

        vtmp = _mm256_mul_pd( vA0_3, vb3 );
        va0_3b_3 = _mm256_add_pd( va0_3b_3, vtmp );

        vtmp = _mm256_mul_pd( vA4_7, vb3 );
        va4_7b_3 = _mm256_add_pd( va4_7b_3, vtmp );

        a += 16;
        b += 8;

    }


    for( i = 0; i < k_left; ++i )
    {
        // Iteration 0.

        // Load va0_3
        va0_3 = _mm256_load_pd( a );
        // Load va4_7
        va4_7 = _mm256_load_pd( a + 4 );

        // Load vb (b0,b1,b2,b3)
        vb = _mm256_load_pd( b );

        vtmp = _mm256_mul_pd( va0_3, vb );
        va0_3b_0 = _mm256_add_pd( va0_3b_0, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb );
        va4_7b_0 = _mm256_add_pd( va4_7b_0, vtmp );

        // Shuffle vb (b1,b0,b3,b2)
        vb = _mm256_shuffle_pd( vb, vb, 0x5 );

        vtmp = _mm256_mul_pd( va0_3, vb );
        va0_3b_1 = _mm256_add_pd( va0_3b_1, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb );
        va4_7b_1 = _mm256_add_pd( va4_7b_1, vtmp );

        // Permute vb (b3,b2,b1,b0)
        vb = _mm256_permute2f128_pd( vb, vb, 0x1 );

        vtmp = _mm256_mul_pd( va0_3, vb );
        va0_3b_2 = _mm256_add_pd( va0_3b_2, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb );
        va4_7b_2 = _mm256_add_pd( va4_7b_2, vtmp );

        // Shuffle vb (b3,b2,b1,b0)
        vb = _mm256_shuffle_pd( vb, vb, 0x5 );

        vtmp = _mm256_mul_pd( va0_3, vb );
        va0_3b_3 = _mm256_add_pd( va0_3b_3, vtmp );

        vtmp = _mm256_mul_pd( va4_7, vb );
        va4_7b_3 = _mm256_add_pd( va4_7b_3, vtmp );

        a += 8;
        b += 4;

    }



    vbeta = _mm256_broadcast_sd( beta );


    __m256d vtmpa_0_3b_0 = _mm256_blend_pd( va0_3b_0, va0_3b_1, 0x6 );
    __m256d vtmpa_0_3b_1 = _mm256_blend_pd( va0_3b_1, va0_3b_0, 0x6 );

    __m256d vtmpa_0_3b_2 = _mm256_blend_pd( va0_3b_2, va0_3b_3, 0x6 );
    __m256d vtmpa_0_3b_3 = _mm256_blend_pd( va0_3b_3, va0_3b_2, 0x6 );

    __m256d vtmpa_4_7b_0 = _mm256_blend_pd( va4_7b_0, va4_7b_1, 0x6 );
    __m256d vtmpa_4_7b_1 = _mm256_blend_pd( va4_7b_1, va4_7b_0, 0x6 );

    __m256d vtmpa_4_7b_2 = _mm256_blend_pd( va4_7b_2, va4_7b_3, 0x6 );
    __m256d vtmpa_4_7b_3 = _mm256_blend_pd( va4_7b_3, va4_7b_2, 0x6 );


    valpha = _mm256_broadcast_sd( alpha );


    va0_3b0 = _mm256_permute2f128_pd( vtmpa_0_3b_0, vtmpa_0_3b_2, 0x30 );
    va0_3b3 = _mm256_permute2f128_pd( vtmpa_0_3b_2, vtmpa_0_3b_0, 0x30 );

    va0_3b1 = _mm256_permute2f128_pd( vtmpa_0_3b_1, vtmpa_0_3b_3, 0x30 );
    va0_3b2 = _mm256_permute2f128_pd( vtmpa_0_3b_3, vtmpa_0_3b_1, 0x30 );

    va4_7b0 = _mm256_permute2f128_pd( vtmpa_4_7b_0, vtmpa_4_7b_2, 0x30 );
    va4_7b3 = _mm256_permute2f128_pd( vtmpa_4_7b_2, vtmpa_4_7b_0, 0x30 );

    va4_7b1 = _mm256_permute2f128_pd( vtmpa_4_7b_1, vtmpa_4_7b_3, 0x30 );
    va4_7b2 = _mm256_permute2f128_pd( vtmpa_4_7b_3, vtmpa_4_7b_1, 0x30 );



    if( rs_c == 1 )
    {
        // Calculate address
        c00 = ( c + 0*rs_c + 0*cs_c );
        // Load
        //vc0_3_0 = _mm256_load_pd( c + 0*rs_c + 0*cs_c  );
        vc0_3_0 = _mm256_load_pd( c00  );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b0);
        // Scale by beta
        vc0_3_0 = _mm256_mul_pd( vbeta, vc0_3_0 );
        // Add gemm result
        vc0_3_0 = _mm256_add_pd( vc0_3_0, vtmp );
        // Store back to memory
        _mm256_store_pd( c00, vc0_3_0 );

        // Calculate address
        c40 = ( c + 4*rs_c + 0*cs_c );
        // Load
        //vc4_7_0 = _mm256_load_pd( c + 4*rs_c + 0*cs_c  );
        vc4_7_0 = _mm256_load_pd( c40  );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b0);
        // Scale by beta
        vc4_7_0 = _mm256_mul_pd( vbeta, vc4_7_0 );
        // Add gemm result
        vc4_7_0 = _mm256_add_pd( vc4_7_0, vtmp );
        // Store back to memory
        _mm256_store_pd( c40, vc4_7_0 );

        // Calculate address
        c01 = ( c + 0*rs_c + 1*cs_c );
        // Load
        //vc0_3_1 = _mm256_load_pd( c + 0*rs_c + 1*cs_c  );
        vc0_3_1 = _mm256_load_pd( c01  );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b1);
        // Scale by beta
        vc0_3_1 = _mm256_mul_pd( vbeta, vc0_3_1 );
        // Add gemm result
        vc0_3_1 = _mm256_add_pd( vc0_3_1, vtmp );
        // Store back to memory
        _mm256_store_pd( c01, vc0_3_1 );


        // Calculate address
        c41 = ( c + 4*rs_c + 1*cs_c );
        // Load
        //vc4_7_1 = _mm256_load_pd( c + 4*rs_c + 1*cs_c  );
        vc4_7_1 = _mm256_load_pd( c41  );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b1);
        // Scale by beta
        vc4_7_1 = _mm256_mul_pd( vbeta, vc4_7_1 );
        // Add gemm result
        vc4_7_1 = _mm256_add_pd( vc4_7_1, vtmp );
        // Store back to memory
        _mm256_store_pd( c41, vc4_7_1 );

        // Calculate address
        c02 = ( c + 0*rs_c + 2*cs_c );
        // Load
        //vc0_3_2 = _mm256_load_pd( c + 0*rs_c + 2*cs_c  );
        vc0_3_2 = _mm256_load_pd( c02 );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b2);
        // Scale by beta
        vc0_3_2 = _mm256_mul_pd( vbeta, vc0_3_2 );
        // Add gemm result
        vc0_3_2 = _mm256_add_pd( vc0_3_2, vtmp );
        // Store back to memory
        _mm256_store_pd( c02, vc0_3_2 );

        // Calculate address
        c42 = ( c + 4*rs_c + 2*cs_c );
        // Load
        //vc4_7_2 = _mm256_load_pd( c + 4*rs_c + 2*cs_c  );
        vc4_7_2 = _mm256_load_pd( c42 );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b2);
        // Scale by beta
        vc4_7_2 = _mm256_mul_pd( vbeta, vc4_7_2 );
        // Add gemm result
        vc4_7_2 = _mm256_add_pd( vc4_7_2, vtmp );
        // Store back to memory
        _mm256_store_pd( c42, vc4_7_2 );

        // Calculate address
        c03 = ( c + 0*rs_c + 3*cs_c );
        // Load
        //vc0_3_3 = _mm256_load_pd( c + 0*rs_c + 3*cs_c  );
        vc0_3_3 = _mm256_load_pd( c03 );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b3);
        // Scale by beta
        vc0_3_3 = _mm256_mul_pd( vbeta, vc0_3_3 );
        // Add gemm result
        vc0_3_3 = _mm256_add_pd( vc0_3_3, vtmp );
        // Store back to memory
        _mm256_store_pd( c03, vc0_3_3 );

        // Calculate address
        c43 = ( c + 4*rs_c + 3*cs_c );
        // Load
        //vc4_7_3 = _mm256_load_pd( c + 4*rs_c + 3*cs_c  );
        vc4_7_3 = _mm256_load_pd( c43 );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b3);
        // Scale by beta
        vc4_7_3 = _mm256_mul_pd( vbeta, vc4_7_3 );
        // Add gemm result
        vc4_7_3 = _mm256_add_pd( vc4_7_3, vtmp );
        // Store back to memory
        _mm256_store_pd( c43, vc4_7_3 );

    }
    else
    {
        // Calculate address
        c00 = ( c + 0*rs_c + 0*cs_c );
        // Load
        //vc0_3_0 = _mm256_load_pd( c + 0*rs_c + 0*cs_c  );
        vc0_3_0 = _mm256_set_pd( *(c + 3*rs_c + 0*cs_c ),
                                 *(c + 2*rs_c + 0*cs_c ),
                                 *(c + 1*rs_c + 0*cs_c ),
                                 *(c + 0*rs_c + 0*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b0);
        // Scale by beta
        vc0_3_0 = _mm256_mul_pd( vbeta, vc0_3_0 );
        // Add gemm result
        vc0_3_0 = _mm256_add_pd( vc0_3_0, vtmp );
        // Store back to memory
        //_mm256_store_pd( c00, vc0_3_0 );

        aa = _mm256_extractf128_pd( vc0_3_0, 0 ) ;
        bb = _mm256_extractf128_pd( vc0_3_0, 1 ) ;

        _mm_storel_pd( c + 0*rs_c + 0*cs_c, aa );
        _mm_storeh_pd( c + 1*rs_c + 0*cs_c, aa );
        _mm_storel_pd( c + 2*rs_c + 0*cs_c, bb );
        _mm_storeh_pd( c + 3*rs_c + 0*cs_c, bb );

        // Calculate address
        c40 = ( c + 4*rs_c + 0*cs_c );
        // Load
        //vc4_7_0 = _mm256_load_pd( c + 4*rs_c + 0*cs_c  );
        vc4_7_0 = _mm256_set_pd( *(c + 7*rs_c + 0*cs_c ),
                                 *(c + 6*rs_c + 0*cs_c ),
                                 *(c + 5*rs_c + 0*cs_c ),
                                 *(c + 4*rs_c + 0*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b0);
        // Scale by beta
        vc4_7_0 = _mm256_mul_pd( vbeta, vc4_7_0 );
        // Add gemm result
        vc4_7_0 = _mm256_add_pd( vc4_7_0, vtmp );
        // Store back to memory
        //_mm256_store_pd( c40, vc4_7_0 );

        aa = _mm256_extractf128_pd( vc4_7_0, 0 ) ;
        bb = _mm256_extractf128_pd( vc4_7_0, 1 ) ;

        _mm_storel_pd( c + 4*rs_c + 0*cs_c, aa );
        _mm_storeh_pd( c + 5*rs_c + 0*cs_c, aa );
        _mm_storel_pd( c + 6*rs_c + 0*cs_c, bb );
        _mm_storeh_pd( c + 7*rs_c + 0*cs_c, bb );

        // Calculate address
        c01 = ( c + 0*rs_c + 1*cs_c );
        // Load
        //vc0_3_1 = _mm256_load_pd( c + 0*rs_c + 1*cs_c  );
        vc0_3_1 = _mm256_set_pd( *(c + 3*rs_c + 1*cs_c ),
                                 *(c + 2*rs_c + 1*cs_c ),
                                 *(c + 1*rs_c + 1*cs_c ),
                                 *(c + 0*rs_c + 1*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b1);
        // Scale by beta
        vc0_3_1 = _mm256_mul_pd( vbeta, vc0_3_1 );
        // Add gemm result
        vc0_3_1 = _mm256_add_pd( vc0_3_1, vtmp );
        // Store back to memory
        //_mm256_store_pd( c01, vc0_3_1 );

        aa = _mm256_extractf128_pd( vc0_3_1, 0 ) ;
        bb = _mm256_extractf128_pd( vc0_3_1, 1 ) ;

        _mm_storel_pd( c + 0*rs_c + 1*cs_c, aa );
        _mm_storeh_pd( c + 1*rs_c + 1*cs_c, aa );
        _mm_storel_pd( c + 2*rs_c + 1*cs_c, bb );
        _mm_storeh_pd( c + 3*rs_c + 1*cs_c, bb );

        // Calculate address
        c41 = ( c + 4*rs_c + 1*cs_c );
        // Load
        //vc4_7_1 = _mm256_load_pd( c + 4*rs_c + 1*cs_c  );
        vc4_7_1 = _mm256_set_pd( *(c + 7*rs_c + 1*cs_c ),
                                 *(c + 6*rs_c + 1*cs_c ),
                                 *(c + 5*rs_c + 1*cs_c ),
                                 *(c + 4*rs_c + 1*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b1);
        // Scale by beta
        vc4_7_1 = _mm256_mul_pd( vbeta, vc4_7_1 );
        // Add gemm result
        vc4_7_1 = _mm256_add_pd( vc4_7_1, vtmp );
        // Store back to memory
        //_mm256_store_pd( c41, vc4_7_1 );

        aa = _mm256_extractf128_pd( vc4_7_1, 0 ) ;
        bb = _mm256_extractf128_pd( vc4_7_1, 1 ) ;

        _mm_storel_pd( c + 4*rs_c + 1*cs_c, aa );
        _mm_storeh_pd( c + 5*rs_c + 1*cs_c, aa );
        _mm_storel_pd( c + 6*rs_c + 1*cs_c, bb );
        _mm_storeh_pd( c + 7*rs_c + 1*cs_c, bb );

        // Calculate address
        c02 = ( c + 0*rs_c + 2*cs_c );
        // Load
        //vc0_3_2 = _mm256_load_pd( c + 0*rs_c + 2*cs_c  );
        vc0_3_2 = _mm256_set_pd( *(c + 3*rs_c + 2*cs_c ),
                                 *(c + 2*rs_c + 2*cs_c ),
                                 *(c + 1*rs_c + 2*cs_c ),
                                 *(c + 0*rs_c + 2*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b2);
        // Scale by beta
        vc0_3_2 = _mm256_mul_pd( vbeta, vc0_3_2 );
        // Add gemm result
        vc0_3_2 = _mm256_add_pd( vc0_3_2, vtmp );
        // Store back to memory
        //_mm256_store_pd( c02, vc0_3_2 );

        aa = _mm256_extractf128_pd( vc0_3_2, 0 ) ;
        bb = _mm256_extractf128_pd( vc0_3_2, 1 ) ;

        _mm_storel_pd( c + 0*rs_c + 2*cs_c, aa );
        _mm_storeh_pd( c + 1*rs_c + 2*cs_c, aa );
        _mm_storel_pd( c + 2*rs_c + 2*cs_c, bb );
        _mm_storeh_pd( c + 3*rs_c + 2*cs_c, bb );

        // Calculate address
        c42 = ( c + 4*rs_c + 2*cs_c );
        // Load
        //vc4_7_2 = _mm256_load_pd( c + 4*rs_c + 2*cs_c  );
        vc4_7_2 = _mm256_set_pd( *(c + 7*rs_c + 2*cs_c ),
                                 *(c + 6*rs_c + 2*cs_c ),
                                 *(c + 5*rs_c + 2*cs_c ),
                                 *(c + 4*rs_c + 2*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b2);
        // Scale by beta
        vc4_7_2 = _mm256_mul_pd( vbeta, vc4_7_2 );
        // Add gemm result
        vc4_7_2 = _mm256_add_pd( vc4_7_2, vtmp );
        // Store back to memory
        //_mm256_store_pd( c42, vc4_7_2 );

        aa = _mm256_extractf128_pd( vc4_7_2, 0 ) ;
        bb = _mm256_extractf128_pd( vc4_7_2, 1 ) ;

        _mm_storel_pd( c + 4*rs_c + 2*cs_c, aa );
        _mm_storeh_pd( c + 5*rs_c + 2*cs_c, aa );
        _mm_storel_pd( c + 6*rs_c + 2*cs_c, bb );
        _mm_storeh_pd( c + 7*rs_c + 2*cs_c, bb );

        // Calculate address
        c03 = ( c + 0*rs_c + 3*cs_c );
        // Load
        //vc0_3_3 = _mm256_load_pd( c + 0*rs_c + 3*cs_c  );
        vc0_3_3 = _mm256_set_pd( *(c + 3*rs_c + 3*cs_c ),
                                 *(c + 2*rs_c + 3*cs_c ),
                                 *(c + 1*rs_c + 3*cs_c ),
                                 *(c + 0*rs_c + 3*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va0_3b3);
        // Scale by beta
        vc0_3_3 = _mm256_mul_pd( vbeta, vc0_3_3 );
        // Add gemm result
        vc0_3_3 = _mm256_add_pd( vc0_3_3, vtmp );
        // Store back to memory
        //_mm256_store_pd( c03, vc0_3_3 );

        aa = _mm256_extractf128_pd( vc0_3_3, 0 ) ;
        bb = _mm256_extractf128_pd( vc0_3_3, 1 ) ;

        _mm_storel_pd( c + 0*rs_c + 3*cs_c, aa );
        _mm_storeh_pd( c + 1*rs_c + 3*cs_c, aa );
        _mm_storel_pd( c + 2*rs_c + 3*cs_c, bb );
        _mm_storeh_pd( c + 3*rs_c + 3*cs_c, bb );



        // Calculate address
        c43 = ( c + 4*rs_c + 3*cs_c );
        // Load
        //vc4_7_3 = _mm256_load_pd( c + 4*rs_c + 3*cs_c  );
        vc4_7_3 = _mm256_set_pd( *(c + 7*rs_c + 3*cs_c ),
                                 *(c + 6*rs_c + 3*cs_c ),
                                 *(c + 5*rs_c + 3*cs_c ),
                                 *(c + 4*rs_c + 3*cs_c ) );
        // Scale by alpha
        vtmp = _mm256_mul_pd( valpha, va4_7b3);
        // Scale by beta
        vc4_7_3 = _mm256_mul_pd( vbeta, vc4_7_3 );
        // Add gemm result
        vc4_7_3 = _mm256_add_pd( vc4_7_3, vtmp );
        // Store back to memory
        //_mm256_store_pd( c43, vc4_7_3 );

        aa = _mm256_extractf128_pd( vc4_7_3, 0 ) ;
        bb = _mm256_extractf128_pd( vc4_7_3, 1 ) ;

        _mm_storel_pd( c + 4*rs_c + 3*cs_c, aa );
        _mm_storeh_pd( c + 5*rs_c + 3*cs_c, aa );
        _mm_storel_pd( c + 6*rs_c + 3*cs_c, bb );
        _mm_storeh_pd( c + 7*rs_c + 3*cs_c, bb );
    }
}

