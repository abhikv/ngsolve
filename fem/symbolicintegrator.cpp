/*********************************************************************/
/* File:   symbolicintegrator.cpp                                    */
/* Author: Joachim Schoeberl                                         */
/* Date:   August 2015                                               */
/*********************************************************************/
/* 
   Symbolic integrators
*/

#include <fem.hpp>
#include <map>
  
namespace ngfem
{
  
  class ProxyUserData
  {
  public:
    class ProxyFunction * testfunction = nullptr;
    int test_comp;
    class ProxyFunction * trialfunction = nullptr;
    int trial_comp;
    
    const FiniteElement * fel = nullptr;
    const FlatVector<double> * elx;
    map<ProxyFunction*, Matrix<double>> remember;
    LocalHeap * lh;
  };

  Array<int> ProxyFunction ::
  Dimensions() const
  {
    int dim = evaluator->Dim();
    int blockdim = evaluator->BlockDim();
    if (blockdim == 1)
      return Array<int> ({dim});
    else
      return Array<int> ({dim/blockdim, blockdim});
    /*
    auto blockdiffop = dynamic_pointer_cast<BlockDifferentialOperator> (evaluator);
    if (blockdiffop)
      {
        int basedim = blockdiffop->BaseDiffOp()->Dim();
        return Array<int> { basedim, dim/basedim };
      }
    else
      return Array<int> ({ dim });
    */
  }
  
  
  void ProxyFunction ::
  Evaluate (const BaseMappedIntegrationPoint & mip,
            FlatVector<> result) const
  {
    ProxyUserData * ud = (ProxyUserData*)mip.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction without userdata");

    if (!testfunction && ud->fel)
      {
        /*
        if (ud->remember.count ((CoefficientFunction*)this))
          {
            result = ud->remember[(CoefficientFunction*)this];
            return;
          }
        */
        evaluator->Apply (*ud->fel, mip, *ud->elx, result, *ud->lh);
        // ud->remember[(CoefficientFunction*)this] = Vector<> (result);
        return;
      }

    result = 0;
    if (ud->testfunction == this)
      result (ud->test_comp) = 1;
    if (ud->trialfunction == this)
      result (ud->trial_comp) = 1;
  }

  void ProxyFunction ::
  Evaluate (const BaseMappedIntegrationPoint & ip,
            FlatVector<Complex> result) const
  {
    Vector<> result_double(result.Size());
    Evaluate (ip, result_double);
    result = result_double;
  }

  void ProxyFunction ::
  EvaluateDeriv (const BaseMappedIntegrationRule & mir,
                 FlatMatrix<> result,
                 FlatMatrix<> deriv) const
  {
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction");

    deriv = 0;
    result = 0;

    if (!testfunction && ud->fel)
      evaluator->Apply (*ud->fel, mir, *ud->elx, result, *ud->lh);

    if (ud->testfunction == this)
      result.Col(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      deriv.Col(ud->trial_comp) = 1;
  }


  void ProxyFunction ::
  EvaluateDDeriv (const BaseMappedIntegrationRule & mir,
                  FlatMatrix<> result,
                  FlatMatrix<> deriv,
                  FlatMatrix<> dderiv) const
  {
    static Timer t("ProxyFunction :: Evaluate", 2);
    static Timer t2("ProxyFunction :: Evaluate, calc only", 2);
    RegionTimer reg(t);
    
    ProxyUserData * ud = (ProxyUserData*)mir.GetTransformation().userdata;
    if (!ud) 
      throw Exception ("cannot evaluate ProxyFunction");

    result = 0;
    deriv = 0;
    dderiv = 0;

    if (!testfunction && ud->fel)
      {
        RegionTimer reg2(t2);
        if (ud->remember.count (const_cast<ProxyFunction*>(this)))
          result = ud->remember[const_cast<ProxyFunction*>(this)];
        else
          evaluator->Apply (*ud->fel, mir, *ud->elx, result, *ud->lh);
      }
    if (ud->testfunction == this)
      deriv.Col(ud->test_comp) = 1;
    if (ud->trialfunction == this)
      deriv.Col(ud->trial_comp) = 1;
  }
  


  SymbolicLinearFormIntegrator ::
  SymbolicLinearFormIntegrator(shared_ptr<CoefficientFunction> acf, VorB avb)
    : cf(acf), vb(avb)
  {
    if (cf->Dimension() != 1)
      throw Exception ("SymbolicLFI needs scalar-valued CoefficientFunction");
    cf->TraverseTree
      ([&] (CoefficientFunction & nodecf)
       {
         auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
         if (proxy && !proxies.Contains(proxy))
           proxies.Append (proxy);
       });
  }

  
  void 
  SymbolicLinearFormIntegrator ::
  CalcElementVector (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatVector<double> elvec,
                     LocalHeap & lh) const
  {
    T_CalcElementVector (fel, trafo, elvec, lh);
  }
  
  void 
  SymbolicLinearFormIntegrator ::
  CalcElementVector (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatVector<Complex> elvec,
                     LocalHeap & lh) const
  {
    T_CalcElementVector (fel, trafo, elvec, lh);
  }
  
  template <typename SCAL> 
  void SymbolicLinearFormIntegrator ::
  T_CalcElementVector (const FiniteElement & fel,
                       const ElementTransformation & trafo, 
                       FlatVector<SCAL> elvec,
                       LocalHeap & lh) const
  {
    HeapReset hr(lh);
    IntegrationRule ir(trafo.GetElementType(), 2*fel.Order());
    BaseMappedIntegrationRule & mir = trafo(ir, lh);
    
    FlatVector<SCAL> elvec1(elvec.Size(), lh);
    
    FlatMatrix<SCAL> values(ir.Size(), 1, lh);
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    
    elvec = 0;
    for (auto proxy : proxies)
      {
        FlatMatrix<SCAL> proxyvalues(ir.Size(), proxy->Dimension(), lh);
          for (int k = 0; k < proxy->Dimension(); k++)
            {
              ud.testfunction = proxy;
              ud.test_comp = k;
              
              cf -> Evaluate (mir, values);
              
              for (int i = 0; i < mir.Size(); i++)
                values.Row(i) *= mir[i].GetWeight();
              proxyvalues.Col(k) = values.Col(0);
            }
          
          proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, elvec1, lh);
          elvec += elvec1;
      }
  }
  
  


  SymbolicBilinearFormIntegrator ::
  SymbolicBilinearFormIntegrator (shared_ptr<CoefficientFunction> acf, VorB avb,
                                  bool aelement_boundary)
    : cf(acf), vb(avb), element_boundary(aelement_boundary)
  {
    if (cf->Dimension() != 1)
        throw Exception ("SymblicBFI needs scalar-valued CoefficientFunction");
    
    cf->TraverseTree
      ( [&] (CoefficientFunction & nodecf)
        {
          auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
          if (proxy) 
            {
              if (proxy->IsTestFunction())
                {
                  if (!test_proxies.Contains(proxy))
                    test_proxies.Append (proxy);
                }
              else
                {                                         
                  if (!trial_proxies.Contains(proxy))
                    trial_proxies.Append (proxy);
                }
            }
        });
  }
  
  
  void 
  SymbolicBilinearFormIntegrator ::
  CalcElementMatrix (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatMatrix<double> elmat,
                     LocalHeap & lh) const
  {
    T_CalcElementMatrix<double> (fel, trafo, elmat, lh);
  }
  
  void 
  SymbolicBilinearFormIntegrator ::
  CalcElementMatrix (const FiniteElement & fel,
                     const ElementTransformation & trafo, 
                     FlatMatrix<Complex> elmat,
                     LocalHeap & lh) const
  {
    if (fel.ComplexShapes())
      T_CalcElementMatrix<Complex,Complex> (fel, trafo, elmat, lh);
    else
      T_CalcElementMatrix<Complex,double> (fel, trafo, elmat, lh);
  }
  
  
  template <typename SCAL, typename SCAL_SHAPES>
  void SymbolicBilinearFormIntegrator ::
  T_CalcElementMatrix (const FiniteElement & fel,
                       const ElementTransformation & trafo, 
                       FlatMatrix<SCAL> elmat,
                       LocalHeap & lh) const
    
  {
    static Timer t("symbolicBFI - CalcElementMatrix", 2);
    static Timer td("symboliBFI - CalcElementMatrix dmats", 2);
    static Timer tb("symboliBFI - CalcElementMatrix diffops", 2);
    static Timer tlapack("symboliBFI - CalcElementMatrix lapack", 2);
    RegionTimer reg(t);

    if (element_boundary)
      {
        switch (trafo.SpaceDim())
          {
          case 1:
            T_CalcElementMatrixEB<1,SCAL, SCAL_SHAPES> (fel, trafo, elmat, lh);
            return;
          case 2:
            T_CalcElementMatrixEB<2,SCAL, SCAL_SHAPES> (fel, trafo, elmat, lh);
            return;
          case 3:
            T_CalcElementMatrixEB<3,SCAL, SCAL_SHAPES> (fel, trafo, elmat, lh);
            return;
          default:
            throw Exception ("Illegal space dimension" + ToString(trafo.SpaceDim()));
          }
      }


    int trial_difforder = 99, test_difforder = 99;
    for (auto proxy : trial_proxies)
      trial_difforder = min2(trial_difforder, proxy->Evaluator()->DiffOrder());
    for (auto proxy : test_proxies)
      test_difforder = min2(test_difforder, proxy->Evaluator()->DiffOrder());

    int intorder = 2*fel.Order();
    auto et = trafo.GetElementType();
    // if (et == ET_TRIG || et == ET_TET)
    // intorder -= test_difforder+trial_difforder;
    IntegrationRule ir(trafo.GetElementType(), intorder);
    BaseMappedIntegrationRule & mir = trafo(ir, lh);
    
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    
    elmat = 0;
        
    for (auto proxy1 : trial_proxies)
      for (auto proxy2 : test_proxies)
        {
          HeapReset hr(lh);

          Tensor<3,SCAL> proxyvalues(mir.Size(), proxy2->Dimension(), proxy1->Dimension());
          td.Start();
          FlatMatrix<SCAL> val(mir.Size(), 1, lh);
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              {
                ud.trialfunction = proxy1;
                ud.trial_comp = k;
                ud.testfunction = proxy2;
                ud.test_comp = l;
                
                cf -> Evaluate (mir, val);
                proxyvalues(STAR,l,k) = val.Col(0);
              }
          td.Stop();
              
          for (int i = 0; i < mir.Size(); i++)
            proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
          
          t.AddFlops (double (mir.Size()) * proxy1->Dimension()*elmat.Width()*elmat.Height());

          IntRange r1 = proxy1->Evaluator()->UsedDofs(fel);
          IntRange r2 = proxy2->Evaluator()->UsedDofs(fel);
          FlatMatrix<SCAL_SHAPES,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
          FlatMatrix<SCAL_SHAPES,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);
          int i = 0;

          enum { BS = 16 };
          for ( ; i+BS <= mir.Size(); i+=BS)
            {
              HeapReset hr(lh);
              FlatMatrix<SCAL,ColMajor> bdbmat1(BS*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<SCAL,ColMajor> bbmat2(BS*proxy2->Dimension(), elmat.Height(), lh);
              tb.Start();
              for (int j = 0; j < BS; j++)
                {
                  int ii = i+j;
                  IntRange r1 = proxy1->Dimension() * IntRange(j,j+1);
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel, mir[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel, mir[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }
              tb.Stop();
              tlapack.Start();
              // elmat += Trans (bbmat2) * bdbmat1 | Lapack;
              elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
              tlapack.Stop();
            }
          
          
          if (i < mir.Size())
            {
              HeapReset hr(lh);
              int rest = mir.Size()-i;
              FlatMatrix<SCAL,ColMajor> bdbmat1(rest*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<SCAL,ColMajor> bbmat2(rest*proxy2->Dimension(), elmat.Height(), lh);

              tb.Start();
              for (int j = 0; j < rest; j++)
                {
                  int ii = i+j;
                  IntRange r1 = proxy1->Dimension() * IntRange(j,j+1);
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel, mir[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel, mir[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }
              tb.Stop();
              tlapack.Start();
              // elmat += Trans (bbmat2) * bdbmat1 | Lapack;
              elmat.Rows(r2).Cols(r1) += Trans (bbmat2.Cols(r2)) * bdbmat1.Cols(r1) | Lapack;
              tlapack.Stop();
            }
        }
  }
  
  
  

  template <int D, typename SCAL, typename SCAL_SHAPES>
  void SymbolicBilinearFormIntegrator ::
  T_CalcElementMatrixEB (const FiniteElement & fel,
                           const ElementTransformation & trafo, 
                           FlatMatrix<SCAL> elmat,
                           LocalHeap & lh) const
      
    {
      elmat = 0;

      auto eltype = trafo.GetElementType();
      int nfacet = ElementTopology::GetNFacets(eltype);

      Facet2ElementTrafo transform(eltype); 
      FlatVector< Vec<D> > normals = ElementTopology::GetNormals<D>(eltype);

      for (int k = 0; k < nfacet; k++)
        {
          HeapReset hr(lh);
          ngfem::ELEMENT_TYPE etfacet = ElementTopology::GetFacetType (eltype, k);
        
          Vec<D> normal_ref = normals[k];
        
          IntegrationRule ir_facet(etfacet, 2*fel.Order());
          IntegrationRule & ir_facet_vol = transform(k, ir_facet, lh);
          MappedIntegrationRule<D,D> mir(ir_facet_vol, trafo, lh);
          
          
          ProxyUserData ud;
          const_cast<ElementTransformation&>(trafo).userdata = &ud;
          
          for (int i = 0; i < mir.Size(); i++)
            {
              auto & mip = mir[i];
              Mat<D> inv_jac = mip.GetJacobianInverse();
              double det = mip.GetMeasure();
              Vec<D> normal = det * Trans (inv_jac) * normal_ref;       
              double len = L2Norm (normal);    // that's the surface measure 
              normal /= len;                   // normal vector on physical element

              mip.SetNV(normal);
              
              for (auto proxy1 : trial_proxies)
                for (auto proxy2 : test_proxies)
                  {
                    HeapReset hr(lh);
                    FlatMatrix<SCAL> proxyvalues(proxy2->Dimension(), proxy1->Dimension(), lh);
                    for (int k = 0; k < proxy1->Dimension(); k++)
                      for (int l = 0; l < proxy2->Dimension(); l++)
                        {
                          ud.trialfunction = proxy1;
                          ud.trial_comp = k;
                          ud.testfunction = proxy2;
                          ud.test_comp = l;
                      
                          Vec<1,SCAL> result;
                          cf->Evaluate (mip, result);
                          proxyvalues(l,k) = ir_facet[i].Weight() * len * result(0);
                        }
                    
                    FlatMatrix<SCAL_SHAPES,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
                    FlatMatrix<SCAL,ColMajor> dbmat1(proxy2->Dimension(), elmat.Width(), lh);
                    FlatMatrix<SCAL_SHAPES,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);
                    
                    proxy1->Evaluator()->CalcMatrix(fel, mip, bmat1, lh);
                    proxy2->Evaluator()->CalcMatrix(fel, mip, bmat2, lh);
                    
                    dbmat1 = proxyvalues * bmat1;
                    elmat += Trans (bmat2) * dbmat1;
                  }
            }
        }
    }


  



  
  SymbolicEnergy :: SymbolicEnergy (shared_ptr<CoefficientFunction> acf,
                                    VorB avb)
    : cf(acf), vb(avb)
  {
    if (cf->Dimension() != 1)
      throw Exception ("SymblicEnergy needs scalar-valued CoefficientFunction");
    
    
    cf->TraverseTree
      ( [&] (CoefficientFunction & nodecf)
        {
          auto proxy = dynamic_cast<ProxyFunction*> (&nodecf);
          if (proxy) 
            {
              if (!proxy->IsTestFunction())
                {                                         
                  if (!trial_proxies.Contains(proxy))
                    trial_proxies.Append (proxy);
                }
            }
        });
  }
  

  void 
  SymbolicEnergy :: CalcLinearizedElementMatrix (const FiniteElement & fel,
                                                 const ElementTransformation & trafo, 
                                                 FlatVector<double> elveclin,
                                                 FlatMatrix<double> elmat,
                                                 LocalHeap & lh) const
  {
    static Timer t("symbolicenergy - calclinearized", 2);
    static Timer td("symbolicenergy - calclinearized dmats", 2);
    RegionTimer reg(t);
    
    IntegrationRule ir(trafo.GetElementType(), 2*fel.Order());
    BaseMappedIntegrationRule & mir = trafo(ir, lh);

    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    ud.fel = &fel;
    ud.elx = &elveclin;
    ud.lh = &lh;
    for (ProxyFunction * proxy : trial_proxies)
      {
        ud.remember[proxy] = Matrix<> (ir.Size(), proxy->Dimension());
        proxy->Evaluator()->Apply(fel, mir, elveclin, ud.remember[proxy], lh);
      }
    
    FlatMatrix<> val(mir.Size(), 1,lh), deriv(mir.Size(), 1,lh), dderiv(mir.Size(), 1,lh);
    
    elmat = 0;
    


    FlatArray<FlatMatrix<>> diags(trial_proxies.Size(), lh);
    for (int k1 : Range(trial_proxies))
      {
        auto proxy = trial_proxies[k1];
        diags[k1].AssignMemory(mir.Size(), proxy->Dimension(), lh);
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.trialfunction = proxy;
            ud.trial_comp = k;
            ud.testfunction = proxy;
            ud.test_comp = k;
            cf -> EvaluateDDeriv (mir, val, deriv, dderiv);

            diags[k1].Col(k) = dderiv.Col(0);
          }
      }
           
    
    for (int k1 : Range(trial_proxies))
      for (int l1 : Range(trial_proxies))
        {
          auto proxy1 = trial_proxies[k1];
          auto proxy2 = trial_proxies[l1];
          td.Start();
          Tensor<3> proxyvalues(mir.Size(), proxy2->Dimension(), proxy1->Dimension());
          
          for (int k = 0; k < proxy1->Dimension(); k++)
            for (int l = 0; l < proxy2->Dimension(); l++)
              {
                ud.trialfunction = proxy1;
                ud.trial_comp = k;
                ud.testfunction = proxy2;
                ud.test_comp = l;
                
                cf -> EvaluateDDeriv (mir, val, deriv, dderiv);
                proxyvalues(STAR,l,k) = dderiv.Col(0);
                
                if (proxy1 != proxy2 || k != l)  // computed mixed second derivatives
                  {
                    proxyvalues(STAR,l,k) -= diags[k1].Col(k);
                    proxyvalues(STAR,l,k) -= diags[l1].Col(l);
                    proxyvalues(STAR,l,k) *= 0.5;
                  }
              }
          td.Stop();

          /*
          for (int i = 0; i < mir.Size(); i++)
            {
              HeapReset hr(lh);
              proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
              
              FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> dbmat1(proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);
              
              proxy1->Evaluator()->CalcMatrix(fel, mir[i], bmat1, lh);
              proxy2->Evaluator()->CalcMatrix(fel, mir[i], bmat2, lh);
              dbmat1 = proxyvalues(i,STAR,STAR) * bmat1;
              elmat += Trans (bmat2) * dbmat1 | Lapack;
            }
          */
          
          for (int i = 0; i < mir.Size(); i++)
            proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();

          t.AddFlops (double (mir.Size()) * proxy1->Dimension()*elmat.Width()*elmat.Height());

          FlatMatrix<double,ColMajor> bmat1(proxy1->Dimension(), elmat.Width(), lh);
          FlatMatrix<double,ColMajor> bmat2(proxy2->Dimension(), elmat.Height(), lh);
          int i = 0;

          enum { BS = 16 };
          for ( ; i+BS <= mir.Size(); i+=BS)
            {
              HeapReset hr(lh);
              FlatMatrix<double,ColMajor> bdbmat1(BS*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(BS*proxy2->Dimension(), elmat.Height(), lh);

              for (int j = 0; j < BS; j++)
                {
                  int ii = i+j;
                  IntRange r1 = proxy1->Dimension() * IntRange(j,j+1);
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel, mir[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel, mir[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }
              elmat += Trans (bbmat2) * bdbmat1 | Lapack;
            }


          if (i < mir.Size())
            {
              HeapReset hr(lh);
              int rest = mir.Size()-i;
              FlatMatrix<double,ColMajor> bdbmat1(rest*proxy2->Dimension(), elmat.Width(), lh);
              FlatMatrix<double,ColMajor> bbmat2(rest*proxy2->Dimension(), elmat.Height(), lh);
              
              for (int j = 0; j < rest; j++)
                {
                  int ii = i+j;
                  IntRange r1 = proxy1->Dimension() * IntRange(j,j+1);
                  IntRange r2 = proxy2->Dimension() * IntRange(j,j+1);
                  proxy1->Evaluator()->CalcMatrix(fel, mir[ii], bmat1, lh);
                  proxy2->Evaluator()->CalcMatrix(fel, mir[ii], bmat2, lh);
                  bdbmat1.Rows(r2) = proxyvalues(ii,STAR,STAR) * bmat1;
                  bbmat2.Rows(r2) = bmat2;
                }
              elmat += Trans (bbmat2) * bdbmat1 | Lapack;
            }
          /*
          for ( ; i < mir.Size(); i++)
            {
              HeapReset hr(lh);
              // proxyvalues(i,STAR,STAR) *= mir[i].GetWeight();
              
              FlatMatrix<double,ColMajor> dbmat1(proxy2->Dimension(), elmat.Width(), lh);
              
              proxy1->Evaluator()->CalcMatrix(fel, mir[i], bmat1, lh);
              proxy2->Evaluator()->CalcMatrix(fel, mir[i], bmat2, lh);
              dbmat1 = proxyvalues(i,STAR,STAR) * bmat1;
              elmat += Trans (bmat2) * dbmat1 | Lapack;
            }
          */
        }
  }
  
  
  
  double SymbolicEnergy :: Energy (const FiniteElement & fel, 
                                   const ElementTransformation & trafo, 
                                   FlatVector<double> elx, 
                                   LocalHeap & lh) const
  {
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    ud.fel = &fel;
    ud.elx = &elx;
    ud.lh = &lh;

    IntegrationRule ir(trafo.GetElementType(), 2*fel.Order());
    BaseMappedIntegrationRule & mir = trafo(ir, lh);

    FlatMatrix<> values(mir.Size(), 1, lh);
    cf -> Evaluate(mir, values);

    double sum = 0;
    for (int i = 0; i < mir.Size(); i++)
      sum += mir[i].GetWeight() * values(i,0);
    return sum;
  }

  void
  SymbolicEnergy :: ApplyElementMatrix (const FiniteElement & fel, 
                                        const ElementTransformation & trafo, 
                                        const FlatVector<double> elx, 
                                        FlatVector<double> ely,
                                        void * precomputed,
                                        LocalHeap & lh) const
  {
    ProxyUserData ud;
    const_cast<ElementTransformation&>(trafo).userdata = &ud;
    ud.fel = &fel;
    ud.elx = &elx;
    ud.lh = &lh;

    HeapReset hr(lh);
    IntegrationRule ir(trafo.GetElementType(), 2*fel.Order());
    BaseMappedIntegrationRule & mir = trafo(ir, lh);
      
    ely = 0;
    FlatVector<> ely1(ely.Size(), lh);

    FlatMatrix<> val(mir.Size(), 1,lh), deriv(mir.Size(), 1,lh);
      
    for (auto proxy : trial_proxies)
      {
        HeapReset hr(lh);
        FlatMatrix<> proxyvalues(mir.Size(), proxy->Dimension(), lh);
        for (int k = 0; k < proxy->Dimension(); k++)
          {
            ud.trialfunction = proxy;
            ud.trial_comp = k;
            cf -> EvaluateDeriv (mir, val, deriv);
            proxyvalues.Col(k) = deriv.Col(0);
          }
        
        for (int i = 0; i < mir.Size(); i++)
          proxyvalues.Row(i) *= mir[i].GetWeight();
        
        proxy->Evaluator()->ApplyTrans(fel, mir, proxyvalues, ely1, lh);
        ely += ely1;
      }
  }
  
  
}
