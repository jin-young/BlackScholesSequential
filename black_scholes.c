#include "black_scholes.h"
#include "gaussian.h"
#include "mock_gaussian.h"
#include "random.h" 
#include "timer.h"
#include "util.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

double shared_mean = 0;
pthread_mutex_t m;
extern rnd_mode;

/**
 * This function is what you compute for each iteration of
 * Black-Scholes.  You don't have to understand it; just call it.
 * "gaussian_random_number" is the current random number (from a
 * Gaussian distribution, which in our case comes from gaussrand1()).
 */
static inline double 
black_scholes_value (const double S,
		     const double E,
		     const double r,
		     const double sigma,
		     const double T,
		     const double gaussian_random_number)
{
  const double current_value = S * exp ( (r - ((sigma*sigma)*0.5)) * T + sigma * sqrt (T) * gaussian_random_number );
//double re = exp (-r * T) * ((current_value - E < 0.0) ? 0.0 : current_value - E);
//printf("in bs_value: %lf\n", re);
//  return re;
  return exp (-r * T) * ((current_value - E < 0.0) ? 0.0 : current_value - E);
  /* return exp (-r * T) * max_double (current_value - E, 0.0); */
}

static inline double 
mock_black_scholes_value (const double S,
		     const double E,
		     const double r,
		     const double sigma,
		     const double T,
		     const double gaussian_random_number)
{
  //const double current_value = S * gaussian_random_number;
  //return  ((current_value - E < 0.0) ? 0.0 : current_value - E);
  return  gaussian_random_number;
  /* return exp (-r * T) * max_double (current_value - E, 0.0); */
}

/**
 * Compute the standard deviation of trials[0 .. M-1].
 */
static double
black_scholes_stddev (void* the_args)
{
  black_scholes_args_t* args = (black_scholes_args_t*) the_args;
  const double mean = args->mean;
  const int M = args->M;
  double variance = 0.0;
  int k;

  for (k = 0; k < M; k++)
    {
      const double diff = args->trials[k] - mean;
      /*
       * Just like when computing the mean, we scale each term of this
       * sum in order to avoid overflow.
       */
      variance += diff * diff / (double) (M-1);
    }

  args->variance = variance;
  return sqrt (variance);
}


// Created by Okido: thread kernel
void*
black_scholes_thread (void* the_args)
{
  wrapper_black_scholes_args_t* wargs = (wrapper_black_scholes_args_t*)the_args;
  black_scholes_args_t* args = wargs->black_sh;

  /* Unpack the IN/OUT struct */

  /* IN (read-only) parameters */
  const int S = args->S; 
  const int E = args->E;
  const long M = args->M;
  const double r = args->r;
  const double sigma = args->sigma;
  const double T = args->T;
  
  const int nthreads = args->nthreads;
  // pid : thread id
  const int pid = wargs->pid; 

  /* OUT (write-only) parameters */
  double* trials = args->trials;
  
  // thread local part
  double mean = 0.0;

  /* Temporary variables */
  gaussrand_state_t gaussrand_state;
  void* prng_stream = NULL; 
  int k;
  double t0, t1;

  /* Spawn a random number generator */
  t0 = get_seconds ();

  prng_stream = spawn_prng_stream (pid); // pid

  t1 = get_seconds ();
  args->prng_stream_spawn_time = t1 - t0;

  /* Initialize the Gaussian random number module for this thread */
  init_gaussrand_state (&gaussrand_state);
  
  /* Do the Black-Scholes iterations */

  double* fixedRands = wargs->fixed_rands;  
  const int gid = pid*(M/nthreads);
  assert (M%nthreads == 0);
  // M/nthreads
  for (k = 0; k < M/nthreads; k++)
    {
      double gaussian_random_number;

      if(rnd_mode == 2) {
          gaussian_random_number = fixedRands[gid+k];
      } else if(rnd_mode == 1) {
          gaussian_random_number = mock_gaussrand_only1 ();
      } else {
          gaussian_random_number = gaussrand1 (&uniform_random_double,
                              prng_stream,
                              &gaussrand_state);
      }

      // pad : pid * nthreads
      int pad = pid * (M/nthreads);
      double trial = 0;
      trial = black_scholes_value (S, E, r, sigma, T, 
				       gaussian_random_number);

// error found: somehow floating type turns out to be integer type
// lost floating number under .0
//printf("trial: %f\n", (double)trial);
      trials[k + pad] = trial;
      /*
       * We scale each term of the sum in order to avoid overflow. 
       * This ensures that mean is never larger than the max
       * element of trials[0 .. M-1].
       */
      mean = mean + (double)trials[k + pad] /(double)M;// / ((double) M/ (double) nthreads);

//printf("trials[k+pad]: %f\n", (double)trials[k+pad]);
//printf("in thread<trial/M>: %f\n", (double)trials[k+pad]/(double)M);
//printf("in thread<in>(mean): %f\n", (double)mean);
    }

//printf("in thread<out>(mean): %f\n", mean);
  /* Pack the OUT values into the args struct */

  double* means = wargs->thread_means;
  means[pid] = mean;
  
  /* 
   * We do the standard deviation computation as a second operation.
   */

  free_prng_stream (prng_stream);
  return NULL;
  pthread_exit(NULL);
}

static void*
black_scholes_kernel (void* the_args, double* fixedRands)
{
  int i;
  black_scholes_args_t* args = (black_scholes_args_t*) the_args;

  int nthreads = args->nthreads;
  double* thread_means = (double*)malloc(sizeof(double)*nthreads);
  pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t) * nthreads);
  wrapper_black_scholes_args_t* wargs_arr = (wrapper_black_scholes_args_t*)malloc(sizeof(wrapper_black_scholes_args_t)*nthreads);

  // create threads
  for (i = 0; i < nthreads; i++) {
    wargs_arr[i].black_sh = args;
    wargs_arr[i].pid = i;
    wargs_arr[i].thread_means = thread_means;
    wargs_arr[i].fixed_rands = fixedRands;

    pthread_create(&threads[i], NULL, &black_scholes_thread, &(wargs_arr[i]));
  } 

  // join threads: barrier
  for (i = 0; i < nthreads; i++) {
    pthread_join(threads[i], NULL);
  }

  // combine results from each threads
  for (i = 0; i < nthreads; i++) {
printf("thread_mean: %lf\n", thread_means[i]);
    args->mean += thread_means[i];
  }

  // calculate average
  //args->mean /= args->M;

  // free the memories
  free(threads);
  free(thread_means);
  free(wargs_arr);
  return NULL;
}


void
black_scholes (confidence_interval_t* interval,
	       double* prng_stream_spawn_time,
	       const double S,
	       const double E,
	       const double r,
	       const double sigma,
	       const double T,
	       const long M,
               const int nthreads,
	       double* fixedRands)
{
  //puts("passed");
  black_scholes_args_t args;
  double mean = 0.0;
  double stddev = 0.0;
  double conf_width = 0.0;
  double* trials = NULL;

  assert (M > 0);
  trials = (double*) malloc (M * sizeof (double));
  assert (trials != NULL);

  args.S = S;
  args.E = E;
  args.r = r;
  args.sigma = sigma;
  args.T = T;
  args.M = M;
  args.trials = trials;
  args.mean = 0.0;
  args.variance = 0.0;
  args.nthreads = nthreads;

  (void) black_scholes_kernel (&args, fixedRands);
  mean = args.mean;
printf("mean: %.60lf\n", mean);
  stddev = black_scholes_stddev (&args);
printf("stddev: %.60lf\n", stddev);

  conf_width = 1.96 * stddev / sqrt ((double) M);
  interval->min = mean - conf_width;
  interval->max = mean + conf_width;
  *prng_stream_spawn_time = args.prng_stream_spawn_time;

  /* Clean up and exit */
    
  deinit_black_scholes_args (&args);
  free(args.trials);
}


void
deinit_black_scholes_args (black_scholes_args_t* args)
{
  if (args != NULL)
    if (args->trials != NULL)
      {
	free (args->trials);
	args->trials = NULL;
      }
}

