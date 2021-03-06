// ngscxx generate_mat_kernels.cpp ; a.out

#include "../include/ngs_stdcpp_include.hpp"
#define NGS_DLL_HEADER

using namespace std;

#include "../ngstd/simd.hpp"
using namespace ngstd;

enum OP { ADD, SUB, SET, SETNEG };

string ToString (OP op)
{
  switch (op)
    {
    case SET: return "SET";
    case SETNEG: return "SETNEG";
    case ADD: return "ADD";
    case SUB: return "SUB";
    }
}

/*
  C = A * B
  C += A * B
  C -= A * B

  A ... h x n
  B ... n x w*SIMD.Size
 */
void GenerateMultAB (ostream & out, int h, int w, OP op, bool aligned_b)
{
  out << "template <> INLINE void MatKernelMultAB<" << h << ", " << w << ", " << ToString(op) << ">" << endl;
  out << "    (size_t n," << endl
      << "     double * pa, size_t da," << endl
      << "     " << (aligned_b ? "SIMD<double>" : "double") << " * pb, size_t db," << endl
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();" << endl;

  if (op == SET || op == SETNEG)
    {
      for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
          out << "SIMD<double> sum" << i << j << "(0);" << endl;
    }
  else
    {
      out << "double * hpc = pc;" << endl;
      for (int i = 0; i < h; i++)
        {
          for (int j = 0; j < w; j++)
            out << "SIMD<double> sum" << i << j << "(pc+SW*" << j << ");" << endl;
          out << "pc += dc;" << endl;
        }
      out << "pc = hpc;" << endl;
    }
  
  out << "for (size_t i = 0; i < n; i++, pa++, pb += db) {" << endl;
  if (aligned_b)
    for (int i = 0; i < w; i++)
      out << "SIMD<double> b" << i << " = pb[" << i << "];" << endl;
  else
    for (int i = 0; i < w; i++)
      out << "SIMD<double> b" << i << "(pb+" << i << "*SW);" << endl;

  for (int i = 0; i < h; i++)
    {
      out << "SIMD<double> a" << i << "(pa["<< i << "*da]);" << endl;
      for (int j = 0; j < w; j++)
        if (op == ADD || op == SET)
          out << "FMAasm(a"<<i<<",b" << j << ",sum" << i << j << ");" << endl;
        else
          out << "sum" << i << j << " -= a" << i << " * b" << j << ";" << endl;
    }
  out << "}" << endl;

  for (int i = 0; i < h; i++)
    {
      for (int j = 0; j < w; j++)
        out << "sum"<< i << j << ".Store(pc+SW*" << j << ");" << endl;
      out << "pc += dc;" << endl;
    }
  
  out << "}" << endl;
}


void GenerateMultAB (ostream & out, int h, int w)
{
  GenerateMultAB (out, h, w, SET, false);
  GenerateMultAB (out, h, w, SETNEG, false);
  GenerateMultAB (out, h, w, ADD, false);
  GenerateMultAB (out, h, w, SUB, false);
  GenerateMultAB (out, h, w, SET, true);
  GenerateMultAB (out, h, w, SETNEG, true);
  GenerateMultAB (out, h, w, ADD, true);
  GenerateMultAB (out, h, w, SUB, true);
}





/*
  C = A * B
  C += A * B
  C -= A * B

  A ... h x n
  B ... n x w*SIMD.Size
 */
void AlignedGenerateMultAB (ostream & out, int h, int w, OP op)
{
  
  out << "template <> inline void MatKernelAlignedMultAB<" << h << ", " << w << ">" << endl
      << "    (size_t n," << endl
      << "     double * pa, size_t da," << endl
      << "     SIMD<double> * pb, size_t db," << endl
      << "     SIMD<double> * pc, size_t dc)" << endl
      << "{" << endl;

  if (op == SET || op == SETNEG)
    {
      for (int i = 0; i < h; i++)
        for (int j = 0; j < w; j++)
          out << "SIMD<double> sum" << i << j << "(0);" << endl;
    }
  else
    {
      out << "SIMD<double> * hpc = pc;" << endl;
      for (int i = 0; i < h; i++)
        {
          for (int j = 0; j < w; j++)
            out << "SIMD<double> sum" << i << j << "(pc+" << j << ");" << endl;
          out << "pc += dc;" << endl;
        }
      out << "pc = hpc;" << endl;
    }
  
  out << "for (size_t i = 0; i < n; i++, pa++, pb += db) {" << endl;
  for (int i = 0; i < w; i++)
    out << "SIMD<double> b" << i << "(pb[" << i << "]);" << endl;

  for (int i = 0; i < h; i++)
    {
      out << "SIMD<double> a" << i << "(pa["<< i << "*da]);" << endl;
      for (int j = 0; j < w; j++)
        if (op == ADD || op == SET)
          out << "FMAasm(a"<<i<<",b" << j << ",sum" << i << j << ");" << endl;
        else
          out << "sum" << i << j << " -= a" << i << " * b" << j << ";" << endl;          
    }
  out << "}" << endl;

  for (int i = 0; i < h; i++)
    {
      for (int j = 0; j < w; j++)
        // out << "sum"<< i << j << ".Store(pc+" << j << ");" << endl;
        out << "pc[" << j << "]= sum"  << i << j << ";" << endl;
      out << "pc += dc;" << endl;
    }
  
  out << "}" << endl;
}




void GenerateMultABMask (ostream & out, int h, OP op, bool aligned_b)
{
  out << "template <> inline void MatKernelMultABMask<" << h << ", " << ToString(op) << ">" << endl;
    
  out << "    (size_t n, SIMD<mask64> mask," << endl
      << "     double * pa, size_t da," << endl
      << "     " << (aligned_b ? "SIMD<double>" : "double") << " * pb, size_t db," << endl    
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  // out << "constexpr int SW = SIMD<double>::Size();" << endl;

  if (op == SET || op == SETNEG)
    {
      for (int i = 0; i < h; i++)
        out << "SIMD<double> sum" << i << "(0);" << endl;
    }
  else
    {
      out << "double * hpc = pc;" << endl;
      for (int i = 0; i < h; i++)
        {
          out << "SIMD<double> sum" << i << "(pc, mask);" << endl;
          out << "pc += dc;" << endl;
        }
      out << "pc = hpc;" << endl;
    }
  
  out << "for (size_t i = 0; i < n; i++, pa++, pb += db) {" << endl;
  out << "SIMD<double> b((double*)pb,mask);" << endl;

  for (int i = 0; i < h; i++)
    {
      out << "SIMD<double> a" << i << "(pa["<< i << "*da]);" << endl;
      if (op == SET || op == ADD)
        out << "FMAasm(a"<<i<<",b,sum" << i << ");" << endl;
      else
        out << "sum" << i << " -= a" << i << "*b;" << endl;
    }
  out << "}" << endl;

  for (int i = 0; i < h; i++)
    {
      out << "sum"<< i << ".Store(pc,mask);" << endl;
      out << "pc += dc;" << endl;
    }
  
  out << "}" << endl;
}

void GenerateMultABMask (ostream & out, int h)
{
  GenerateMultABMask (out, h, SET, false);
  GenerateMultABMask (out, h, SETNEG, false);
  GenerateMultABMask (out, h, ADD, false);
  GenerateMultABMask (out, h, SUB, false);
  GenerateMultABMask (out, h, SET, true);
  GenerateMultABMask (out, h, SETNEG, true);
  GenerateMultABMask (out, h, ADD, true);
  GenerateMultABMask (out, h, SUB, true);
}


/*
  C = A * B^t
  A ... h x n
  B ... w * n
 */
void GenerateScalAB (ostream & out, int h, int w, bool simded)
{
  out << "template <> INLINE auto MatKernelScalAB<" << h << ", " << w << ">" << endl
      << "    (size_t n," << endl
      << "     " << (simded ? "SIMD<double>" : "double") << " * pa, size_t da," << endl
      << "     " << (simded ? "SIMD<double>" : "double") << " * pb, size_t db)" << endl
      << "{" << endl;
  if (!simded)
    out << "constexpr int SW = SIMD<double>::Size();" << endl;

  for (int i = 0; i < h; i++)
    for (int j = 0; j < w; j++)
      out << "SIMD<double> sum" << i << j << "(0);" << endl;

  out << "size_t i = 0;" << endl;
  if (!simded)
    out << "for ( ; i+SW <= n; i+=SW) {" << endl;
  else
    out << "for ( ; i < n; i++) {" << endl;
  
  for (int i = 0; i < h; i++)
    if (simded)
      out << "SIMD<double> a" << i << "(pa[" << i << "*da+i]);" << endl;
    else
      out << "SIMD<double> a" << i << "(pa+" << i << "*da+i);" << endl;
  // for (int i = 0; i < w; i++)
  // out << "SIMD<double> b" << i << "(pb+" << i << "*db+i);" << endl;

  for (int j = 0; j < w; j++)
    {
      if (simded)
        out << "SIMD<double> b" << j << "(pb[" << j << "*db+i]);" << endl;
      else
        out << "SIMD<double> b" << j << "(pb+" << j << "*db+i);" << endl;    
      for (int i = 0; i < h; i++)
        {
          if (h*w < 12)  // with 12 we are on the limit of registers -> fmaasm better
            out << "sum" << i << j << " += a" << i << " * b" << j << ";" << endl;
          else
            out << "FMAasm(a"<<i<<",b" << j << ",sum" << i << j << ");" << endl;
        }
    }
  out << "}" << endl;

  if (!simded)
    {
      out << "size_t r = n % SW;" << endl;
      out << "if (r) {" << endl;
      out << "SIMD<mask64> mask(r);" << endl;
      for (int i = 0; i < h; i++)
        out << "SIMD<double> a" << i << "(pa+" << i << "*da+i, mask);" << endl;
      
      for (int j = 0; j < w; j++)
        {
          out << "SIMD<double> b" << j << "(pb+" << j << "*db+i, mask);" << endl;
          for (int i = 0; i < h; i++)
            out << "FMAasm(a"<<i<<",b" << j << ",sum" << i << j << ");" << endl;
        }
      out << "}" << endl;
    }

  if (w == 1 && (h % 4 == 0))
    {
      out << "return make_tuple(";
      for (int i = 0; i < h; i+=4)
        {
          out << "HSum(sum" << i << "0, sum" << i+1 << "0, sum" << i+2 << "0, sum" << i+3 <<"0)";
          if (i+4 < h) out << ",";
        }
      out << ");"  << endl;
    }

  else

    {
      out << "return make_tuple(";
      for (int i = 0; i < h; i++)
        {
          out << "HSum(";
          for (int j = 0; j < w; j++)
            {
              out << "sum"<< i << j;
              if (j < w-1)
                out << ",";
              else
                out << ")";
            }
          if (i < h-1)
            out << ",";
          else
            out << ");" << endl;
        }
    }
  out << "}" << endl;
}


void GenerateScalAB (ostream & out, int h, int w)
{
  GenerateScalAB(out, h, w, false);
  GenerateScalAB(out, h, w, true);
}


void GenKernel (ofstream & out, int h, int w)
{
  out << "template <> inline void MyScalTrans<" << h << ", " << w << ">" << endl
      << "    (size_t n," << endl
      << "     double * pa, size_t da," << endl
      << "     double * pb, size_t db," << endl
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();" << endl
      << "double * hpc = pc;" << endl;
  for (int i = 0; i < h; i++)
    {
      for (int j = 0; j < w; j++)
        out << "SIMD<double> sum" << i << j << "(pc+SW*" << j << ");" << endl;
      out << "pc += dc;" << endl;
    }
  out << "pc = hpc;" << endl;

  out << "for (size_t i = 0; i < n; i++, pa += da, pb += db) {" << endl;
  for (int i = 0; i < h; i++)
    out << "SIMD<double> a" << i << "(pa[" << i << "]);" << endl;

  for (int i = 0; i < w; i++)
    {
      out << "SIMD<double> b" << i << "(pb+" << i << "*SW);" << endl;
      for (int j = 0; j < h; j++)
        // out << "sum" << j << i << " += a" << j << " * b" << i << ";" << endl;
        out << "FMAasm(b"<<i<<",a" << j << ",sum" << j << i << ");" << endl;
    }
  out << "}" << endl;

  for (int i = 0; i < h; i++)
    {
      for (int j = 0; j < w; j++)
        out << "sum"<< i << j << ".Store(pc+SW*" << j << ");" << endl;
      out << "pc += dc;" << endl;
    }
  
  out << "}" << endl;
}







/*
  C = A * B
  C += A * B
  C -= A * B

  A ... h x w
  B ... w x n
 */
void GenerateDaxpy (ostream & out, int h, int w, OP op, bool aligned_b)
{
  out << "template <> INLINE void MatKernelDaxpy<" << h << ", " << w << ", " << ToString(op) << ">" << endl;
  out << "    (size_t n," << endl
      << "     double * pa, size_t da," << endl
      << "     " << (aligned_b ? "SIMD<double>" : "double") << " * pb, size_t db," << endl
      << "     " << (aligned_b ? "SIMD<double>" : "double") << " * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();" << endl;

  for (int i = 0; i < h; i++)
    for (int j = 0; j < w; j++)
      out << "SIMD<double> a" << i << j << "(pa[" << i << "*da+"<< j << "]);" << endl;


  for (int i = 0; i < h; i++)
    out << "double * pc" << i << " = pc+" << i << "*dc;" << endl;
  for (int i = 0; i < w; i++)
    out << "double * pb" << i << " = pb+" << i << "*db;" << endl;

  out << "size_t i = 0;" << endl;
  out << "for ( ; i+SW <= n; i+=SW) {" << endl;
  
  
  if (op == SET || op == SETNEG)
    {
      for (int i = 0; i < h; i++)
        out << "SIMD<double> c" << i << "(0);" << endl;
    }
  else
    {
      for (int i = 0; i < h; i++)
        out << "SIMD<double> c" << i << "(pc" << i << "+i);" << endl;
    }
  
  /*
    if (aligned_b)
    for (int i = 0; i < w; i++)
    out << "SIMD<double> b" << i << " = pb[" << i << "];" << endl;
    else
    for (int i = 0; i < w; i++)
    out << "SIMD<double> b" << i << "(pb+" << i << "*SW);" << endl;
  */
  
  for (int j = 0; j < w; j++)
    {
      out << "SIMD<double> b" << j << "(pb" << j << "+i);" << endl;
      for (int i = 0; i < h; i++)
        if (op == ADD || op == SET)
          out << "c" << i << " += a" << i  << j << " * b" << j << ";" << endl;
        else
          out << "c" << i << " -= a" << i  << j << " * b" << j << ";" << endl;
    }

  for (int i = 0; i < h; i++)
    out << "c" << i << ".Store(pc" << i << "+i);" << endl;
  
  out << "}" << endl;



  out << "SIMD<mask64> mask(n%SW);" << endl;
  if (op == SET || op == SETNEG)
    {
      for (int i = 0; i < h; i++)
        out << "SIMD<double> c" << i << "(0);" << endl;
    }
  else
    {
      for (int i = 0; i < h; i++)
        out << "SIMD<double> c" << i << "(pc" << i << "+i, mask);" << endl;
    }
  
  /*
    if (aligned_b)
    for (int i = 0; i < w; i++)
    out << "SIMD<double> b" << i << " = pb[" << i << "];" << endl;
    else
    for (int i = 0; i < w; i++)
    out << "SIMD<double> b" << i << "(pb+" << i << "*SW);" << endl;
  */
  
  for (int j = 0; j < w; j++)
    {
      out << "SIMD<double> b" << j << "(pb" << j << "+i, mask);" << endl;
      for (int i = 0; i < h; i++)
        if (op == ADD || op == SET)
          out << "c" << i << " += a" << i  << j << " * b" << j << ";" << endl;
        else
          out << "c" << i << " -= a" << i  << j << " * b" << j << ";" << endl;
    }

  for (int i = 0; i < h; i++)
    out << "c" << i << ".Store(pc" << i << "+i, mask);" << endl;

  

  
  out << "}" << endl;
}

void GenerateDaxpy (ostream & out, int h, int w)
{
  GenerateDaxpy (out, h, w, SET, false);
  GenerateDaxpy (out, h, w, ADD, false);
  GenerateDaxpy (out, h, w, SUB, false);
  /*
  GenerateDaxpy (out, h, w, SET, true);
  GenerateDaxpy (out, h, w, ADD, true);
  GenerateDaxpy (out, h, w, SUB, true);
  */
}







void GenerateShortSum (ostream & out, int wa, OP op)
{
  out << "template <> INLINE void MatKernelShortSum<" << wa << ", " << ToString(op) << ">" << endl;
  out << "    (size_t ha, size_t wb," << endl
      << "     double * pa, size_t da," << endl
      << "     double * pb, size_t db," << endl
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();\n" 
      << "for (size_t i = 0; i+SW <= wb; i += SW, pb += SW, pc += SW)\n"
      << "{\n";
  if (wa > 0)
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "(pb2); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  out << "#pragma unroll 1\n";
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n";
  if (op == SET || op == SETNEG)
    out << "SIMD<double> sum = 0.0;\n";
  else
    out << "SIMD<double> sum(pc2);\n";
    
  for (int k = 0; k < wa; k++)
    if (op == SET || op == ADD)
      out << "sum += SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
    else
      out << "sum -= SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
  out << "sum.Store(pc2);\n"
      << "} }\n";

  out << "size_t rest = wb % SW; \n"
      << "if (rest == 0) return; \n"
      << "SIMD<mask64> mask(rest); \n";

  if (wa > 0)
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "(pb2, mask); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";

  out << "#pragma unroll 1\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n";
  if (op == SET || op == SETNEG)
    out << "SIMD<double> sum = 0.0;\n";
  else
    out << "SIMD<double> sum(pc2, mask);\n";
  for (int k = 0; k < wa; k++)
    if (op == SET || op == ADD)
      out << "sum += SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
    else
      out << "sum -= SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";      
  out << "sum.Store(pc2, mask);\n"
      << "} }\n";





  // unroll B width 2

  out << "template <> INLINE void MatKernelShortSum2<" << wa << ", " << ToString(op) << ">" << endl;
  out << "    (size_t ha, size_t wb," << endl
      << "     double * pa, size_t da," << endl
      << "     double * pb, size_t db," << endl
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();\n" 
      << "for (size_t i = 0; i+2*SW <= wb; i += 2*SW, pb += 2*SW, pc += 2*SW)\n"
      << "{\n";
  if (wa > 0)
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "0(pb2);\n"
        << "SIMD<double> b" << k << "1(pb2+SW); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  
  // out << "#pragma unroll 2\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum0 = 0.0;\n"
      << "SIMD<double> sum1 = 0.0;\n";
  for (int k = 0; k < wa; k++)
    out << "sum0 += SIMD<double>(pa2[" << k << "]) * b"<< k << "0;\n"
        << "sum1 += SIMD<double>(pa2[" << k << "]) * b"<< k << "1;\n";
  out << "sum0.Store(pc2);\n"
      << "sum1.Store(pc2+SW);\n"
      << "} }\n";
  
  out << "size_t rest = wb % (2*SW); \n"
      << "if (rest == 0) return; \n";

  out << "if (rest >= SW) \n"
      << "{\n"
      << "if (rest > SW)\n"
      << "{\n";

  out << "SIMD<mask64> mask(rest-SW); \n";
  if (wa > 0)
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "0(pb2);\n"
        << "SIMD<double> b" << k << "1(pb2+SW,mask); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";

  out << "#pragma unroll 1\n";    
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum0 = 0.0;\n"
      << "SIMD<double> sum1 = 0.0;\n";
  for (int k = 0; k < wa; k++)
    out << "sum0 += SIMD<double>(pa2[" << k << "]) * b"<< k << "0;\n"
        << "sum1 += SIMD<double>(pa2[" << k << "]) * b"<< k << "1;\n";
  out << "sum0.Store(pc2);\n"
      << "sum1.Store(pc2+SW,mask);\n"
      << "}\n";
    
  out << "return;\n"
      << "}\n";
    
    // rest == SW
  if (wa > 0)
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "(pb2); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  
  out << "#pragma unroll 1\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum = 0.0;\n";
  for (int k = 0; k < wa; k++)
      out << "sum += SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
  out << "sum.Store(pc2);\n"
      << "}\n";
  
  out << "return;\n"
      << "}\n";
  
  
  // rest < SW
  out << "SIMD<mask64> mask(rest); \n";
  if (wa > 0)
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "(pb2, mask); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  
  out << "#pragma unroll 1\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum = 0.0;\n";
  for (int k = 0; k < wa; k++)
      out << "sum += SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
  out << "sum.Store(pc2, mask);\n"
      << "} }\n";




}





void GenerateAtB_SmallWA (ostream & out, int wa, OP op)
{
  out << "template <> INLINE void MatKernelAtB_SmallWA<" << wa << ", " << ToString(op) << ">" << endl;
  out << "    (size_t ha, size_t wb," << endl
      << "     double * pa, size_t da," << endl
      << "     double * pb, size_t db," << endl
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();\n" 
      << "for (size_t i = 0; i+SW <= wb; i += SW, pb += SW, pc += SW)\n"
      << "{\n"
      << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> sum" << k << "(0.0);\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  out << "#pragma unroll 1\n";
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pb2 += db)\n"
      << "{\n";
  out << "SIMD<double> bjk(pb2);\n";
  for (int k = 0; k < wa; k++)
    out << "FMAasm (bjk,SIMD<double>(pa2[" << k << "]), sum" << k <<");\n";
  out << "}\n";
  for (int k = 0; k < wa; k++)
    out << "sum" << k << ".Store(pc2); pc2 += dc;\n";
  out << "}\n";

  out << "size_t rest = wb % SW; \n"
      << "if (rest == 0) return; \n"
      << "SIMD<mask64> mask(rest); \n";
  
  out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> sum" << k << "(0.0);\n";    
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";

  out << "#pragma unroll 1\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pb2 += db)\n"
      << "{\n"
      << "SIMD<double> bjk(pb2, mask);\n";    
  for (int k = 0; k < wa; k++)
    out << "FMAasm (bjk,SIMD<double>(pa2[" << k << "]), sum" << k <<");\n";    
  out << "}\n";
  for (int k = 0; k < wa; k++)
    out << "sum" << k << ".Store(pc2, mask); pc2 += dc;\n";
  
  out << "}\n";


  /*

  // unroll B width 2

  out << "template <> INLINE void MatKernelShortSum2<" << wa << ", " << ToString(op) << ">" << endl;
  out << "    (size_t ha, size_t wb," << endl
      << "     double * pa, size_t da," << endl
      << "     double * pb, size_t db," << endl
      << "     double * pc, size_t dc)" << endl
      << "{" << endl;
  out << "constexpr int SW = SIMD<double>::Size();\n" 
      << "for (size_t i = 0; i+2*SW <= wb; i += 2*SW, pb += 2*SW, pc += 2*SW)\n"
      << "{\n"
      << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "0(pb2);\n"
        << "SIMD<double> b" << k << "1(pb2+SW); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  
  // out << "#pragma unroll 2\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum0 = 0.0;\n"
      << "SIMD<double> sum1 = 0.0;\n";
  for (int k = 0; k < wa; k++)
    out << "sum0 += SIMD<double>(pa2[" << k << "]) * b"<< k << "0;\n"
        << "sum1 += SIMD<double>(pa2[" << k << "]) * b"<< k << "1;\n";
  out << "sum0.Store(pc2);\n"
      << "sum1.Store(pc2+SW);\n"
      << "} }\n";
  
  out << "size_t rest = wb % (2*SW); \n"
      << "if (rest == 0) return; \n";

  out << "if (rest >= SW) \n"
      << "{\n"
      << "if (rest > SW)\n"
      << "{\n";

  out << "SIMD<mask64> mask(rest-SW); \n";    
  out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "0(pb2);\n"
        << "SIMD<double> b" << k << "1(pb2+SW,mask); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";

  out << "#pragma unroll 1\n";    
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum0 = 0.0;\n"
      << "SIMD<double> sum1 = 0.0;\n";
  for (int k = 0; k < wa; k++)
    out << "sum0 += SIMD<double>(pa2[" << k << "]) * b"<< k << "0;\n"
        << "sum1 += SIMD<double>(pa2[" << k << "]) * b"<< k << "1;\n";
  out << "sum0.Store(pc2);\n"
      << "sum1.Store(pc2+SW,mask);\n"
      << "}\n";
    
  out << "return;\n"
      << "}\n";
    
    // rest == SW
    out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "(pb2); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  
  out << "#pragma unroll 1\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum = 0.0;\n";
  for (int k = 0; k < wa; k++)
      out << "sum += SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
  out << "sum.Store(pc2);\n"
      << "}\n";
  
  out << "return;\n"
      << "}\n";
  
  
  // rest < SW
  out << "SIMD<mask64> mask(rest); \n";
  out << "double * pb2 = pb;\n";
  for (int k = 0; k < wa; k++)
    out << "SIMD<double> b" << k << "(pb2, mask); pb2 += db;\n";
  out << "double * pa2 = pa;\n"
      << "double * pc2 = pc;\n"
      << "__assume(ha>0);\n";
  
  out << "#pragma unroll 1\n";  
  out << "for (size_t j = 0; j < ha; j++, pa2 += da, pc2 += dc)\n"
      << "{\n"
      << "SIMD<double> sum = 0.0;\n";
  for (int k = 0; k < wa; k++)
      out << "sum += SIMD<double>(pa2[" << k << "]) * b"<< k << ";\n";
  out << "sum.Store(pc2, mask);\n"
      << "} }\n";
  */

}




void  GenerateMatVec (ostream & out, int wa, OP op)
{
  out << "template <> INLINE void KernelMatVec<" << wa << ", " << ToString(op) << ">" << endl
      << "(size_t ha, double * pa, size_t da, double * x, double * y) {" << endl;

  int SW = SIMD<double>::Size();  // generate optimal code for my host
  // out << "constexpr int SW = SIMD<double>::Size();" << endl;
  int i = 0;
  for ( ; SW*(i+1) <= wa; i++)
    out << "SIMD<double," << SW << "> x" << i << "(x+" << i*SW << ");" << endl;
  
  if (SW == 4 && (wa % SW == 1))
    {
      out << "double x" << i << " = x[" << i*SW << "];" << endl;      
    }
  else if (SW == 4 && (wa % SW == 2))
    {
      out << "SIMD<double,2> x" << i << "(x+" << i*SW << ");" << endl;      
    }
  else if (wa % SW)  // do the mask load :-(
    {
      out << "SIMD<mask64," << SW << "> mask(" << wa%SW << "UL);" << endl;
      out << "SIMD<double," << SW << "> x" << i << "(x+" << i*SW << ", mask);" << endl;
    }
  out << "size_t i = 0;" << endl;
  out << "for ( ; i+4 <= ha; i+=4, pa += 4*da) {" << endl;
  out << "SIMD<double," << SW << "> sum0(0.0), sum1(0.0), sum2(0.0), sum3(0.0);" << endl;
  i = 0;
  for ( ; SW*(i+1) <= wa; i++)
    {
      out << "sum0 += SIMD<double," << SW << ">(pa+" << i*SW << ") * x" << i << ";" << endl;
      out << "sum1 += SIMD<double," << SW << ">(pa+da+" << i*SW << ") * x" << i << ";" << endl;
      out << "sum2 += SIMD<double," << SW << ">(pa+2*da+" << i*SW << ") * x" << i << ";" << endl;
      out << "sum3 += SIMD<double," << SW << ">(pa+3*da+" << i*SW << ") * x" << i << ";" << endl;
    }

  if (SW == 4 && (wa % SW == 1))
    {
      /*
      for (int k = 0; k < 4; k++)
        out << "sum"<<k<< " += SIMD<double,4> (pa[" << k << "*da+" << i*SW << "] * x" << i << ", 0,0,0);" << endl;
      */
      ;
    }
  else if (SW == 4 && (wa % SW == 2))
    {
      out << "SIMD<double,2> zero(0.0);" << endl;
      out << "sum0 += SIMD<double,4> (SIMD<double,2>(pa+" << i*SW << ") * x" << i << ", zero);" << endl;      
      out << "sum1 += SIMD<double,4> (SIMD<double,2>(pa+da+" << i*SW << ") * x" << i << ", zero);" << endl;      
      out << "sum2 += SIMD<double,4> (SIMD<double,2>(pa+2*da+" << i*SW << ") * x" << i << ", zero);" << endl;      
      out << "sum3 += SIMD<double,4> (SIMD<double,2>(pa+3*da+" << i*SW << ") * x" << i << ", zero);" << endl;      
    }
  else if (wa % SW)
    {
      out << "sum0 += SIMD<double," << SW << ">(pa+" << i*SW << ", mask) * x" << i << ";" << endl;
      out << "sum1 += SIMD<double," << SW << ">(pa+da+" << i*SW << ", mask) * x" << i << ";" << endl;
      out << "sum2 += SIMD<double," << SW << ">(pa+2*da+" << i*SW << ", mask) * x" << i << ";" << endl;
      out << "sum3 += SIMD<double," << SW << ">(pa+3*da+" << i*SW << ", mask) * x" << i << ";" << endl;
    }
  out << "SIMD<double,4> vsum = HSum(sum0,sum1,sum2,sum3);" << endl;

  if (SW == 4 && (wa % SW == 1))
    {
      out << "vsum += x" << i << "*SIMD<double,4> ("
          << "pa[0*da+" << i*SW << "], "
          << "pa[1*da+" << i*SW << "], "
          << "pa[2*da+" << i*SW << "], "
          << "pa[3*da+" << i*SW << "]);" << endl;
    }
  
  out << "vsum.Store(y+i);" << endl;
  out << "}" << endl;


  out << "if (ha & 2) {" << endl;
  out << "SIMD<double," << SW << "> sum0(0.0), sum1(0.0);" << endl;
  i = 0;
  for ( ; SW*(i+1) <= wa; i++)
    {
      out << "sum0 += SIMD<double," << SW << ">(pa+" << i*SW << ") * x" << i << ";" << endl;
      out << "sum1 += SIMD<double," << SW << ">(pa+da+" << i*SW << ") * x" << i << ";" << endl;
    }
  
  if (SW == 4 && (wa % SW == 1))
    {
      for (int k = 0; k < 2; k++)
        out << "sum"<<k<< " += SIMD<double,4> (pa[" << k << "*da+" << i*SW << "] * x" << i << ", 0,0,0);" << endl;      
    }
  else if (SW == 4 && (wa % SW == 2))
    {
      out << "SIMD<double,2> zero(0.0);" << endl;
      out << "sum0 += SIMD<double,4> (SIMD<double,2>(pa+" << i*SW << ") * x" << i << ", zero);" << endl;      
      out << "sum1 += SIMD<double,4> (SIMD<double,2>(pa+da+" << i*SW << ") * x" << i << ", zero);" << endl;      
    }
  else if (wa % SW)
    {
      out << "sum0 += SIMD<double," << SW << ">(pa+" << i*SW << ", mask) * x" << i << ";" << endl;
      out << "sum1 += SIMD<double," << SW << ">(pa+da+" << i*SW << ", mask) * x" << i << ";" << endl;
    }
  out << "SIMD<double,2> vsum = HSum(sum0,sum1);" << endl;
  out << "vsum.Store(y+i);" << endl;
  out << "i += 2; pa += 2*da;" << endl;
  out << "}" << endl;
  
  
  out << "if (ha & 1) {" << endl;
  out << "SIMD<double," << SW << "> sum(0.0);" << endl;
  i = 0;
  for ( ; SW*(i+1) <= wa; i++)
    out << "sum += SIMD<double," << SW << ">(pa+" << i*SW << ") * x" << i << ";" << endl;

  
  if (SW == 4 && (wa % SW == 1))
    {
      out << "sum += SIMD<double,4> (pa[" << i*SW << "] * x" << i << ", 0,0,0);" << endl;      
    }
  else if (SW == 4 && (wa % SW == 2))
    {
      out << "SIMD<double,2> zero(0.0);" << endl;
      out << "sum += SIMD<double,4> (SIMD<double,2>(pa+" << i*SW << ") * x" << i << ", zero);" << endl;      
    }
  else if (wa % SW)
    out << "sum += SIMD<double," << SW << ">(pa+" << i*SW << ", mask) * x" << i << ";" << endl;
  out << "y[i] = HSum(sum);" << endl;

  out << "} }" << endl;
}





int main ()
{
  ofstream out("matkernel.hpp");

  out << "enum OPERATION { ADD, SUB, SET, SETNEG };" << endl;

  out << "template <size_t H, size_t W, OPERATION OP>" << endl
      << "inline void MatKernelMultAB" << endl
      << "(size_t n, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);" << endl;
  out << "template <size_t H, size_t W, OPERATION OP>" << endl
      << "inline void MatKernelMultAB" << endl
      << "(size_t n, double * pa, size_t da, SIMD<double> * pb, size_t db, double * pc, size_t dc);" << endl;
  out << "template <size_t H, size_t W>" << endl
      << "inline void MatKernelAlignedMultAB" << endl
      << "(size_t n, double * pa, size_t da, SIMD<double> * pb, size_t db, SIMD<double> * pc, size_t dc);" << endl;

  for (int i = 1; i <= 3; i++)
    {
      GenerateMultAB (out, 1, i);  
      GenerateMultAB (out, 2, i);
      GenerateMultAB (out, 3, i);
      GenerateMultAB (out, 4, i);
      GenerateMultAB (out, 5, i);
      GenerateMultAB (out, 6, i);
      
      AlignedGenerateMultAB (out, 1, i, SET);  
      AlignedGenerateMultAB (out, 2, i, SET);
      AlignedGenerateMultAB (out, 3, i, SET);
      AlignedGenerateMultAB (out, 4, i, SET);
      AlignedGenerateMultAB (out, 5, i, SET);
      AlignedGenerateMultAB (out, 6, i, SET);
    }
  
  GenerateMultAB (out, 8, 1);
  GenerateMultAB (out, 12, 1);
  

  out << "template <size_t H, OPERATION OP>" << endl
      << "inline void MatKernelMultABMask" << endl
      << "(size_t n, SIMD<mask64> mask, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);" << endl;
  out << "template <size_t H, OPERATION OP>" << endl
      << "inline void MatKernelMultABMask" << endl
      << "(size_t n, SIMD<mask64> mask, double * pa, size_t da, SIMD<double> * pb, size_t db, double * pc, size_t dc);" << endl;

  GenerateMultABMask (out, 1);  
  GenerateMultABMask (out, 2);
  GenerateMultABMask (out, 3);
  GenerateMultABMask (out, 4);
  GenerateMultABMask (out, 5);
  GenerateMultABMask (out, 6);

  
  // Scal AB
  
  out << "template <size_t H, size_t W> inline auto MatKernelScalAB" << endl
      << "    (size_t n," << endl
      << "     double * pa, size_t da," << endl
      << "     double * pb, size_t db);" << endl;
  out << "template <size_t H, size_t W> inline auto MatKernelScalAB" << endl
      << "    (size_t n," << endl
      << "     SIMD<double> * pa, size_t da," << endl
      << "     SIMD<double> * pb, size_t db);" << endl;

  GenerateScalAB (out, 6, 4);  
  GenerateScalAB (out, 3, 4);  
  GenerateScalAB (out, 1, 4);
  GenerateScalAB (out, 6, 2);  
  GenerateScalAB (out, 3, 2);  
  GenerateScalAB (out, 8, 1);  
  GenerateScalAB (out, 6, 1);  
  GenerateScalAB (out, 4, 1);  
  GenerateScalAB (out, 3, 1);  
  GenerateScalAB (out, 2, 1);  
  GenerateScalAB (out, 1, 1);  


  
  out << "template <size_t H, size_t W>" << endl
      << "inline void MyScalTrans" << endl
      << "(size_t n, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);" << endl;
  
  GenKernel (out, 1, 4);
  GenKernel (out, 2, 4);
  GenKernel (out, 3, 4);
  GenKernel (out, 4, 4);
  GenKernel (out, 5, 4);
  GenKernel (out, 6, 4);

  out << "template <size_t H, size_t W, OPERATION OP>" << endl
      << "inline void MatKernelDaxpy" << endl
      << "(size_t n, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);" << endl;
  out << "template <size_t H, size_t W, OPERATION OP>" << endl
      << "inline void MatKernelDaxpy" << endl
      << "(size_t n, double * pa, size_t da, SIMD<double> * pb, size_t db, SIMD<double> * pc, size_t dc);" << endl;

  for (int i = 0; i <= 12; i++)
    GenerateDaxpy (out, 1, i);

  GenerateDaxpy (out, 2, 1);  
  GenerateDaxpy (out, 2, 2);  
  GenerateDaxpy (out, 2, 3);  
  GenerateDaxpy (out, 2, 4);  
  GenerateDaxpy (out, 3, 1);  
  GenerateDaxpy (out, 3, 2);  
  GenerateDaxpy (out, 3, 3);  
  GenerateDaxpy (out, 3, 4);

  out << "// C = A * B,  with short inner loop\n"
      << "template <size_t WA, OPERATION OP>\n"
      << "inline void MatKernelShortSum\n"
      << "(size_t ha, size_t wb, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);\n";

  out << "// C = A * B,  with short inner loop, unroll width B\n"
      << "template <size_t WA, OPERATION OP>\n"
      << "inline void MatKernelShortSum2\n"
      << "(size_t ha, size_t wb, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);\n";

  for (int i = 0; i <= 12; i++)
    {
      GenerateShortSum (out, i, SET);  
      GenerateShortSum (out, i, ADD);  
    }




  out << "// C = A^t * B,  with short inner loop\n"
      << "template <size_t WA, OPERATION OP>\n"
      << "inline void MatKernelAtB_SmallWA\n"
      << "(size_t ha, size_t wb, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);\n";

  out << "// C = A^t * B,  with short inner loop, unroll width B\n"
      << "template <size_t WA, OPERATION OP>\n"
      << "inline void MatKernelAtB_SmallWA\n"
      << "(size_t ha, size_t wb, double * pa, size_t da, double * pb, size_t db, double * pc, size_t dc);\n";

  for (int i = 0; i <= 12; i++)
    GenerateAtB_SmallWA (out, i, SET);

  out << "// y = A * x,  with fix width" << endl;
  out << "template <size_t WA, OPERATION OP>" << endl
      << "inline void KernelMatVec" << endl
      << "(size_t ha, double * pa, size_t da, double * x, double * y);" << endl;
  for (int i = 0; i <= 24; i++)
    GenerateMatVec (out, i, SET);
}
