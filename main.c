#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "black_scholes.h"
#include "parser.h"
#include "random.h"
#include "timer.h"

extern int rnd_mode = 0;

/**
 * Usage: ./hw1.x <filename> <nthreads>
 *
 * <filename> (don't include the angle brackets) is the name of 
 * a data file in the current directory containing the parameters
 * for the Black-Scholes simulation.  It has exactly six lines 
 * with no white space.  Put each parameter one to a line, with
 * an endline after it.  Here are the parameters:
 *
 * S
 * E
 * r
 * sigma
 * T
 * M
 *
 * <nthreads> (don't include the angle brackets) is the number of
 * worker threads to use at a time in the benchmark.  The sequential
 * code which we supply to you doesn't use this argument; your code
 * will.
 */
int
main (int argc, char* argv[])
{
  confidence_interval_t interval;
  double S, E, r, sigma, T;
  long M = 0;
  char* filename = NULL;
  int nthreads = 1;
  double t1, t2;
  int i;
  int debug_mode = 0;
  
  if (argc < 5)
    {
      fprintf (stderr, 
         "Usage: ./hw1.x <filename> <trials:M>  <nthreads> [rnd_mode] [debug_mode]\n\n");
      exit (EXIT_FAILURE);
    }
  filename = argv[1];
  nthreads = to_int (argv[3]);
  rnd_mode = to_int(argv[4]);
  if (argv[5]!= NULL) {
    debug_mode = to_int(argv[5]);
  }

  parse_parameters (&S, &E, &r, &sigma, &T, &M, filename);

  M = to_long (argv[2]);

  /* rearrange nthread and M(trials) 
   * for the further use, we arrange M based on the number of threads
   * if M is not, or less than nthreads.
   * */
  if (M < 256) {
     printf("Trials(M) is less than minimum requirement, 256,\n"
            "So, we increase M to 256\n");
     M = 256;
  }
  if (nthreads > M) {
     printf("The number of threads is exceed to M\n"
            "So, we set it to M\n");
     nthreads = M;
  }
  if (M % nthreads) {
   M = (M/nthreads+1)*nthreads;
   printf("nthreads and M is not balanced\n"
          "So, we rebalance M to muliple of nthreads\n"
          "M: %ld, nthreads: %d\n", M, nthreads);
  }


  /*
   * generate pre-generated random numbers
   * */
  double* preRands = (double*)malloc (sizeof (double) * M);
  if (preRands == NULL) {
    printf("ERROR: Cannot allocate size of memory: %ld\n"
           "Begin with smaller size of M.\n", sizeof(double)*M);
    exit(1);
  }
  for (i = 0; i < M; i++) {
    preRands[i] = i /(double)M;
    if (debug_mode > 0 && (i < 10 || i > M-10))
      printf("RND%d: %.6lf, ", i, preRands[i]);
  }
  /* 
   * Make sure init_timer() is only called by one thread,
   * before all the other threads run!
   */
  init_timer ();

  /* Same goes for initializing the PRNG */
  init_prng (random_seed ());

  /*
   * Run the benchmark and time it.
   */

  t1 = get_seconds ();
  void** prng_stream = (void**)malloc(sizeof(void*)*nthreads);
  for( i = 0; i < nthreads; i++) {
    prng_stream[i] = spawn_prng_stream (i);
  }
  double prng_stream_spawn_time = get_seconds() - t1;
  /* 
   * In the parallel case, you may want to set prng_stream_spawn_time to 
   * the max of all the prng_stream_spawn_times, or just take a representative
   * sample... 
   */
  bs_return_t ret = black_scholes (&interval,
                  S, E, r, sigma, T, M, nthreads, preRands, prng_stream, debug_mode);
  t2 = get_seconds ();

  /*
   * A fun fact about C string literals (i.e., strings enclosed in
   * double quotes) is that the C preprocessor automatically
   * concatenates them if they are separated only by whitespace.
   */

  if (nthreads == 1) {
     printf ("Black-Scholes (Ver. Sequential) benchmark:\n");
  }
  else if (nthreads > 1) {
     printf ("Black-Scholes (Ver. Threads: %d) benchmark:\n", nthreads);
  }
  printf(
      "--------------------------------------------\n"
      "Trials                       	%ld\n"
      "Confidence interval:(%g, %g)\n"
      "Average Trials(BS)          :	%10lf\n"
      "Standard Deviation          :	%10lf\n"
      "--------------------------------------------\n"
      "Total simulation time (sec) :	%10lf\n"
      "PRNG stream spawn time (sec):	%10lf\n"
      "BS computation time (sec)   :	%10lf\n\n"
      //"S                           %g\n"
      //"E                           %g\n"
      //"r                           %g\n"
      //"sigma                       %g\n"
      //"T                           %g\n"
      //S, E, r, sigma, T
      , M
      , interval.min, interval.max
      , ret.mean
      , ret.stddev
      , t2 - t1
      , prng_stream_spawn_time
      , (t2 - t1) - prng_stream_spawn_time);
 
  free(preRands);
  free(prng_stream);
  return 0;
}



