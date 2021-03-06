#include "FFT.h"
#include "oclFFT.h"
#include <vector>
#include <cassert>
#include <iostream>
#include <ctime>
#include <sys/resource.h>

#define PI 3.14159265

using namespace std;

FFT::FFT(int n, bool inverse)
    : n(n), inverse(inverse), result(vector<Complex>(n))
{
    lgN = 0;
    for (int i = n; i > 1; i >>= 1)
    {
        ++lgN;
        assert((i & 1) == 0);
    }
    omega.resize(lgN);
    int m = 1;
    for (int s = 0; s < lgN; ++s)
    {
        m <<= 1;
        if (inverse)
            omega[s] = exp(Complex(0, 2.0 * PI / m));
        else
        {
            Complex tmp = Complex(0, -2.0 * PI / m);
            omega[s] = exp(tmp);
//            cout << "Omega (" << real(omega[s]) << "," << imag(omega[s]) << "), tmp (" << real(tmp) << "," << imag(tmp) << ")" << endl;

        }
    }
}

double getcputime(void)        
{
  struct timeval tim;        
  struct rusage ru;        
  getrusage(RUSAGE_SELF, &ru);        
  tim=ru.ru_utime;        
  double t=(double)tim.tv_sec*1000000 + (double)tim.tv_usec;        
  tim=ru.ru_stime;        
  t+=(double)tim.tv_sec*1000000 + (double)tim.tv_usec;        
  return t; 
}

std::vector<FFT::Complex> FFT::transform(const vector<Complex>& buf)
{
    bitReverseCopy(buf, result);

//    for(int i = 0; i < n; i++)
//    {
//      cout << "Index " << i << ": (before) " << real(result[i]) << " " << imag(result[i]) << endl;
//    }
//    cout << endl;

    int m = 1;

    start_t = getcputime();
    for (int s = 0; s < lgN; ++s)
    {
        m <<= 1;
        for (int k = 0; k < n; k += m)
        {
            Complex current_omega = 1;
            for (int j = 0; j < (m >> 1); ++j)
            {
//                cout << "Current omega (" << real(current_omega) << "," << imag(current_omega) << ") for j = " << j << endl;
//                cout << "Omega (" << real(omega[s]) << "," << imag(omega[s]) << ")" << endl;
                Complex t = current_omega * result[k + j + (m >> 1)];
                Complex u = result[k + j];
                result[k + j] = u + t;
                result[k + j + (m >> 1)] = u - t;
                current_omega *= omega[s];
            }
        }
//        for(int i = 0; i < n; i++)
//        {
//          cout << "Index " << i << ": (after) (s:" << s << ") " << real(result[i]) << " " << imag(result[i]) << endl;
//        }
//        cout << endl;

    }

    end_t = getcputime();
    clock_diff = end_t - start_t;
    shrLog("CPU transform start microseconds\t %5.2f \n", start_t);
    shrLog("CPU transform end microseconds\t %5.2f \n", end_t);
    shrLog("CPU transform diff microseconds\t %5.2f \n", clock_diff);

    
//    for(int i = 0; i < n; i++)
//    {
//      cout << "Index " << i << ": (after) " << real(result[i]) << " " << imag(result[i]) << endl;
//    }
//    cout << endl;

    if (inverse == false)
        for (int i = 0; i < n; ++i)
            result[i] /= n;

    return result;
}

void FFT::transformGPU(const vector<Complex>& buf, void * cl_buf, void * cl_debug_buf, cl_mem cmDev, 
                       cl_mem cmPointsPerGroup, cl_mem cmDebug, cl_mem cmDir, cl_kernel ckKernel, size_t szGlobalWorkSize, size_t szLocalWorkSize, unsigned int points_per_group,
                       cl_command_queue cqCommandQueue, cl_int ciErr, int argc, const char **argv)
{
  int dir_i = (inverse) ? -1 : 1;
  void * dir = (void *)&dir_i;
  void * pts_per_grp_p = (void *)&points_per_group;
  bitReverseCopy(buf, result);
  cl_float2 * cl_float2_buf = (cl_float2 *)cl_buf;
  cl_float2 * cl_float2_debug_buf = (cl_float2 *)cl_debug_buf;
  for(int i = 0; i < n; i++)
  {
    cl_float2_buf[i].x = (float)real(result[i]);
    cl_float2_buf[i].y = (float)imag(result[i]);
    cl_float2_debug_buf[i].x = -1.0;
    cl_float2_debug_buf[i].y = -1.0;
  }

//  for(int i = 0; i < n; i++)
//  {
//    cout << "Index " << i << ": (before) " << cl_float2_buf[i].x << " " << cl_float2_buf[i].y << endl;
//  }
//  cout << endl;

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmDev, CL_FALSE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmPointsPerGroup, CL_FALSE, 0, sizeof(cl_uint), pts_per_grp_p, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmDebug, CL_FALSE, 0, sizeof(cl_float2) * n, cl_debug_buf, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmDir, CL_FALSE, 0, sizeof(cl_int), dir, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }



//  for(int s = 0; s < lgN; ++s)
//  {
//    m <<= 1;
//    szLocalWorkSize = m >> 1;

//    cout << "Enqueue with Global Work Size " << szGlobalWorkSize << " and Local Work Size " << szLocalWorkSize << endl;
//    if(s == 0)
//    {
      // Launch kernel
//      ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, &start_event);
    start_t = getcputime();

    ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
      shrLog("Error is %s\n", oclErrorString(ciErr));
      Cleanup(argc, (char **)argv, EXIT_FAILURE);
    }
//    }
//    else if(s == (lgN-1))
//    {
      // Launch kernel
//      ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, &end_event);
//      if (ciErr != CL_SUCCESS)
//      {
//        shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//        shrLog("Error is %s\n", oclErrorString(ciErr));
//        Cleanup(argc, (char **)argv, EXIT_FAILURE);
//      }
//    }
//    else
//    {
       // Launch kernel
//      ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL);
//      if (ciErr != CL_SUCCESS)
//      {
//        shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//        shrLog("Error is %s\n", oclErrorString(ciErr));
//        Cleanup(argc, (char **)argv, EXIT_FAILURE);
//      }
//    }

    clFinish(cqCommandQueue);

    ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
      Cleanup(argc, (char **)argv, EXIT_FAILURE);
    } 

    // one more iteration to combine all the elements together
    if(n > points_per_group)
    {
      int m = points_per_group;
      for(int s = log2(points_per_group); s < lgN; ++s)
      {
        m <<= 1;
//        cout << "Performing CPU calculation with s: " << s << " and m: " << m << endl;
        for(int k = 0; k < n; k+= m)
        {
          cl_float2 current_omega;
          current_omega.s0 = 1.0;
          current_omega.s1 = 0.0;
          for (int j = 0; j < (m >> 1); ++j)
          {
            cl_float2 t; 
            t.s0 = current_omega.s0 * cl_float2_buf[k + j + (m >> 1)].s0 - current_omega.s1 * cl_float2_buf[k + j + (m >> 1)].s1;
            t.s1 = current_omega.s0 * cl_float2_buf[k + j + (m >> 1)].s1 + current_omega.s1 * cl_float2_buf[k + j + (m >> 1)].s0;
            cl_float2 u = cl_float2_buf[k + j];
            cl_float2_buf[k + j].s0 = u.s0 + t.s0;
            cl_float2_buf[k + j].s1 = u.s1 + t.s1;
            cl_float2_buf[k + j + (m >> 1)].s0 = u.s0 - t.s0;
            cl_float2_buf[k + j + (m >> 1)].s1 = u.s1 - t.s1;
            current_omega.s0 = current_omega.s0 * real(omega[s]) - current_omega.s1 * imag(omega[s]) ;
            current_omega.s1 = current_omega.s0 * imag(omega[s]) + current_omega.s1 * real(omega[s]) ;
          }
        }
      }
    }

    end_t = getcputime();
    clock_diff = end_t - start_t;
    shrLog("GPU transform start microseconds\t %5.2f \n", start_t);
    shrLog("GPU transform end microseconds\t %5.2f \n", end_t);
    shrLog("GPU transform diff microseconds\t %5.2f \n", clock_diff);

    ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDebug, CL_TRUE, 0, sizeof(cl_float2) * n, cl_debug_buf, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
     Cleanup(argc, (char **)argv, EXIT_FAILURE);
    } 

//    ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
//    if (ciErr != CL_SUCCESS)
//    {
//      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//     Cleanup(argc, (char **)argv, EXIT_FAILURE);
//    } 

    for(int i = 0; i < n; i++)
    {
//      cout << "Index " << i << " (" << cl_float2_debug_buf[i].x << "," << 
//                     		       cl_float2_debug_buf[i].y << ")" << endl;
      cl_float2_debug_buf[i].x = -1;
      cl_float2_debug_buf[i].y = -1;
    }

//  }

//  clGetEventProfilingInfo(start_event, CL_PROFILING_COMMAND_START,
//         sizeof(start_time), &start_time, NULL);
//  clGetEventProfilingInfo(end_event, CL_PROFILING_COMMAND_END,
//         sizeof(end_time), &end_time, NULL);
//  total_time = (double)(end_time - start_time) / 1e3; // convert from nanoseconds to microseconds
//  shrLog("GPU transform time\t %5.2f microseconds \n", total_time);

//  ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
//  if (ciErr != CL_SUCCESS)
//  {
//    shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//    Cleanup(argc, (char **)argv, EXIT_FAILURE);
//  } 

//  for(int i = 0; i < n; i++)
//  {
//    cout << "Index " << i << ": (after) " << cl_float2_buf[i].x << " " << cl_float2_buf[i].y << endl;
//  }
//  cout << endl;

  if(inverse == false)
  {
    for(int i = 0; i < n; ++i)
    {
      cl_float2_buf[i].s0 = cl_float2_buf[i].s0 / n;
      cl_float2_buf[i].s1 = cl_float2_buf[i].s1 / n;
    }
  }

//  clReleaseEvent(start_event);
//  clReleaseEvent(end_event);
}

void FFT::transformAllGPU(const vector<Complex>& buf, void * cl_buf, void * cl_debug_buf, cl_mem cmDev, cl_mem cmM,
                       cl_mem cmPointsPerGroup, cl_mem cmDebug, cl_mem cmDir, cl_kernel ckKernel, cl_kernel ckKernelAll, 
                       size_t szGlobalWorkSize, size_t szLocalWorkSize, unsigned int points_per_group,
                       cl_command_queue cqCommandQueue, cl_int ciErr, int argc, const char **argv)
{
  int dir_i = (inverse) ? -1 : 1;
  void * dir = (void *)&dir_i;
  void * pts_per_grp_p = (void *)&points_per_group;
  bitReverseCopy(buf, result);
  cl_float2 * cl_float2_buf = (cl_float2 *)cl_buf;
  cl_float2 * cl_float2_debug_buf = (cl_float2 *)cl_debug_buf;
  for(int i = 0; i < n; i++)
  {
    cl_float2_buf[i].x = (float)real(result[i]);
    cl_float2_buf[i].y = (float)imag(result[i]);
    cl_float2_debug_buf[i].x = -1.0;
    cl_float2_debug_buf[i].y = -1.0;
  }

//  for(int i = 0; i < n; i++)
//  {
//    cout << "Index " << i << ": (before) " << cl_float2_buf[i].x << " " << cl_float2_buf[i].y << endl;
//  }
//  cout << endl;

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmDev, CL_FALSE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmPointsPerGroup, CL_FALSE, 0, sizeof(cl_uint), pts_per_grp_p, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmDebug, CL_FALSE, 0, sizeof(cl_float2) * n, cl_debug_buf, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }

  ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmDir, CL_FALSE, 0, sizeof(cl_int), dir, 0, NULL, NULL);
  if (ciErr != CL_SUCCESS)
  {
    shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
    Cleanup(argc, (char **)argv, EXIT_FAILURE);
  }



//  for(int s = 0; s < lgN; ++s)
//  {
//    m <<= 1;
//    szLocalWorkSize = m >> 1;

//    cout << "Enqueue with Global Work Size " << szGlobalWorkSize << " and Local Work Size " << szLocalWorkSize << endl;
//    if(s == 0)
//    {
      // Launch kernel
//      ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, &start_event);
    start_t = getcputime();

    ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
      shrLog("Error is %s\n", oclErrorString(ciErr));
      Cleanup(argc, (char **)argv, EXIT_FAILURE);
    }
//    }
//    else if(s == (lgN-1))
//    {
      // Launch kernel
//      ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, &end_event);
//      if (ciErr != CL_SUCCESS)
//      {
//        shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//        shrLog("Error is %s\n", oclErrorString(ciErr));
//        Cleanup(argc, (char **)argv, EXIT_FAILURE);
//      }
//    }
//    else
//    {
       // Launch kernel
//      ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernel, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL);
//      if (ciErr != CL_SUCCESS)
//      {
//        shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//        shrLog("Error is %s\n", oclErrorString(ciErr));
//        Cleanup(argc, (char **)argv, EXIT_FAILURE);
//      }
//    }

    clFinish(cqCommandQueue);

    ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
      Cleanup(argc, (char **)argv, EXIT_FAILURE);
    } 

    // one more iteration to combine all the elements together
    if(n > points_per_group)
    {
      int m = points_per_group;
      
      for(int s = log2(points_per_group); s < lgN; ++s)
      {
        m <<= 1;

        ciErr = clEnqueueWriteBuffer(cqCommandQueue, cmM, CL_FALSE, 0, sizeof(cl_uint), &m, 0, NULL, NULL);
        if (ciErr != CL_SUCCESS)
        {
          shrLog("Error in clEnqueueWriteBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
          Cleanup(argc, (char **)argv, EXIT_FAILURE);
        }

        ciErr = clEnqueueNDRangeKernel(cqCommandQueue, ckKernelAll, 1, NULL, &szGlobalWorkSize, &szLocalWorkSize, 0, NULL, NULL); 
        if (ciErr != CL_SUCCESS)
        {
          shrLog("Error in clEnqueueNDRangeKernel, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
          shrLog("Error is %s\n", oclErrorString(ciErr));
          Cleanup(argc, (char **)argv, EXIT_FAILURE);
        }
        clFinish(cqCommandQueue);
      }

    }

    end_t = getcputime();
    clock_diff = end_t - start_t;
    shrLog("AllGPU transform start microseconds\t %5.2f \n", start_t);
    shrLog("AllGPU transform end microseconds\t %5.2f \n", end_t);
    shrLog("AllGPU transform diff microseconds\t %5.2f \n", clock_diff);

    ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
      Cleanup(argc, (char **)argv, EXIT_FAILURE);
    } 

     ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDebug, CL_TRUE, 0, sizeof(cl_float2) * n, cl_debug_buf, 0, NULL, NULL);
    if (ciErr != CL_SUCCESS)
    {
      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
      Cleanup(argc, (char **)argv, EXIT_FAILURE);
    } 

//    ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
//    if (ciErr != CL_SUCCESS)
//    {
//      shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//     Cleanup(argc, (char **)argv, EXIT_FAILURE);
//    } 

    for(int i = 0; i < n; i++)
    {
//      cout << "Index " << i << " (" << cl_float2_debug_buf[i].x << "," << 
//                     		       cl_float2_debug_buf[i].y << ")" << endl;
      cl_float2_debug_buf[i].x = -1;
      cl_float2_debug_buf[i].y = -1;
    }

//  }

//  clGetEventProfilingInfo(start_event, CL_PROFILING_COMMAND_START,
//         sizeof(start_time), &start_time, NULL);
//  clGetEventProfilingInfo(end_event, CL_PROFILING_COMMAND_END,
//         sizeof(end_time), &end_time, NULL);
//  total_time = (double)(end_time - start_time) / 1e3; // convert from nanoseconds to microseconds
//  shrLog("GPU transform time\t %5.2f microseconds \n", total_time);

//  ciErr = clEnqueueReadBuffer(cqCommandQueue, cmDev, CL_TRUE, 0, sizeof(cl_float2) * n, cl_buf, 0, NULL, NULL);
//  if (ciErr != CL_SUCCESS)
//  {
//    shrLog("Error in clEnqueueReadBuffer, Line %u in file %s !!!\n\n", __LINE__, __FILE__);
//    Cleanup(argc, (char **)argv, EXIT_FAILURE);
//  } 

//  for(int i = 0; i < n; i++)
//  {
//    cout << "Index " << i << ": (after) " << cl_float2_buf[i].x << " " << cl_float2_buf[i].y << endl;
//  }
//  cout << endl;

  if(inverse == false)
  {
    for(int i = 0; i < n; ++i)
    {
      cl_float2_buf[i].s0 = cl_float2_buf[i].s0 / n;
      cl_float2_buf[i].s1 = cl_float2_buf[i].s1 / n;
    }
  }

//  clReleaseEvent(start_event);
//  clReleaseEvent(end_event);
}


float FFT::getIntensity(Complex c)
{
    return abs(c);
}

float FFT::getPhase(Complex c)
{
    return arg(c);
}

void FFT::bitReverseCopy(const vector<Complex>& src, vector<Complex>& dest)
        const
{
    for (int i = 0; i < n; ++i)
    {
        int index = i, rev = 0;
        for (int j = 0; j < lgN; ++j)
        {
            rev = (rev << 1) | (index & 1);
            index >>= 1;
        }
        dest[rev] = src[i];
    }
}
