//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Teuchos_RCP.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Sacado_ParameterRegistration.hpp"

#include "Intrepid_FunctionSpaceTools.hpp"
#include "Aeras_Layouts.hpp"

namespace Aeras {

//**********************************************************************
template<typename EvalT, typename Traits>
XScalarAdvectionResid<EvalT, Traits>::
XScalarAdvectionResid(const Teuchos::ParameterList& p,
              const Teuchos::RCP<Aeras::Layouts>& dl) :
  wBF      (p.get<std::string> ("Weighted BF Name"), dl->node_qp_scalar),
  wGradBF  (p.get<std::string> ("Weighted Gradient BF Name"),dl->node_qp_gradient),
  rho      (p.get<std::string> ("QP Variable Name"), dl->qp_scalar_level),
  rhoGrad  (p.get<std::string> ("Gradient QP Variable Name"), dl->qp_gradient_level),
  rhoDot   (p.get<std::string> ("QP Time Derivative Variable Name"), dl->qp_scalar_level),
  coordVec (p.get<std::string> ("QP Coordinate Vector Name"), dl->qp_gradient),
  Residual (p.get<std::string> ("Residual Name"), dl->node_scalar_level)
{

  Teuchos::ParameterList* xsa_params = p.get<Teuchos::ParameterList*>("XScalarAdvection Problem");
  Re = xsa_params->get<double>("Reynolds Number", 1.0); //Default: Re=1
  std::cout << "XScalarAdvectionResid: Re= " << Re << std::endl;

  this->addDependentField(rho);
  this->addDependentField(rhoGrad);
  this->addDependentField(rhoDot);
  this->addDependentField(wBF);
  this->addDependentField(wGradBF);
  this->addDependentField(coordVec);

  this->addEvaluatedField(Residual);


  this->setName("Aeras::XScalarAdvectionResid"+PHX::TypeString<EvalT>::value);

  std::vector<PHX::DataLayout::size_type> dims;
  wGradBF.fieldTag().dataLayout().dimensions(dims);
  numNodes = dims[1];
  numQPs   = dims[2];
  numDims  = dims[3];

  rho.fieldTag().dataLayout().dimensions(dims);
  numLevels =  p.get< int >("Number of Vertical Levels");
  std::cout << "XScalarAdvectionResid: numLevels= " << numLevels << std::endl;

  // Register Reynolds number as Sacado-ized Parameter
  Teuchos::RCP<ParamLib> paramLib = p.get<Teuchos::RCP<ParamLib> >("Parameter Library");
  new Sacado::ParameterRegistration<EvalT, SPL_Traits>("Reynolds Number", this, paramLib);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void XScalarAdvectionResid<EvalT, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(rho,fm);
  this->utils.setFieldData(rhoGrad,fm);
  this->utils.setFieldData(rhoDot,fm);
  this->utils.setFieldData(wBF,fm);
  this->utils.setFieldData(wGradBF,fm);
  this->utils.setFieldData(coordVec,fm);

  this->utils.setFieldData(Residual,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits>
void XScalarAdvectionResid<EvalT, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  std::vector<ScalarT> vel(numLevels);
  for (std::size_t level=0; level < numLevels; ++level) {
    vel[level] = (level+1)*Re;
  }

  for (std::size_t i=0; i < Residual.size(); ++i) Residual(i)=0.0;

  for (std::size_t cell=0; cell < workset.numCells; ++cell) {
    for (std::size_t qp=0; qp < numQPs; ++qp) {

      for (std::size_t node=0; node < numNodes; ++node) {
        for (std::size_t level=0; level < numLevels; ++level) {
          // Transient Term
          Residual(cell,node,level) += rhoDot(cell,qp,level)*wBF(cell,node,qp);
          // Advection Term
          for (std::size_t j=0; j < numDims; ++j) {
              Residual(cell,node,level) += vel[level]*rhoGrad(cell,qp,level,j)*wBF(cell,node,qp);
          }
        }
      }
    }
  }
}

//**********************************************************************
// Provide Access to Parameter for sensitivity/optimization/UQ
template<typename EvalT,typename Traits>
typename XScalarAdvectionResid<EvalT,Traits>::ScalarT&
XScalarAdvectionResid<EvalT,Traits>::getValue(const std::string &n)
{
  return Re;
}


//**********************************************************************
//**********************************************************************







//**********************************************************************
template<typename Traits>
XScalarAdvectionResid<PHAL::AlbanyTraits::Jacobian, Traits>::
XScalarAdvectionResid(const Teuchos::ParameterList& p,
              const Teuchos::RCP<Aeras::Layouts>& dl) :
  wBF      (p.get<std::string> ("Weighted BF Name"), dl->node_qp_scalar),
  wGradBF  (p.get<std::string> ("Weighted Gradient BF Name"),dl->node_qp_gradient),
  rho      (p.get<std::string> ("QP Variable Name"), dl->qp_scalar_level),
  rhoGrad  (p.get<std::string> ("Gradient QP Variable Name"), dl->qp_gradient_level),
  rhoDot   (p.get<std::string> ("QP Time Derivative Variable Name"), dl->qp_scalar_level),
  coordVec (p.get<std::string> ("QP Coordinate Vector Name"), dl->qp_gradient),
  Residual (p.get<std::string> ("Residual Name"), dl->node_scalar_level)
{

  Teuchos::ParameterList* xsa_params = p.get<Teuchos::ParameterList*>("XScalarAdvection Problem");
  Re = xsa_params->get<double>("Reynolds Number", 1.0); //Default: Re=1
  std::cout << "XScalarAdvectionResid: Re= " << Re << std::endl;

  this->addDependentField(rho);
  this->addDependentField(rhoGrad);
  this->addDependentField(rhoDot);
  this->addDependentField(wBF);
  this->addDependentField(wGradBF);
  this->addDependentField(coordVec);

  this->addEvaluatedField(Residual);


  this->setName("Aeras::XScalarAdvectionResid"+PHX::TypeString<PHAL::AlbanyTraits::Jacobian>::value);

  std::vector<PHX::DataLayout::size_type> dims;
  wGradBF.fieldTag().dataLayout().dimensions(dims);
  numNodes = dims[1];
  numQPs   = dims[2];
  numDims  = dims[3];

  rho.fieldTag().dataLayout().dimensions(dims);
  numLevels =  p.get< int >("Number of Vertical Levels");
  std::cout << "XScalarAdvectionResid: numLevels= " << numLevels << std::endl;

  // Register Reynolds number as Sacado-ized Parameter
  Teuchos::RCP<ParamLib> paramLib = p.get<Teuchos::RCP<ParamLib> >("Parameter Library");
  new Sacado::ParameterRegistration<PHAL::AlbanyTraits::Jacobian, SPL_Traits>("Reynolds Number", this, paramLib);
}

//**********************************************************************
template<typename Traits>
void XScalarAdvectionResid<PHAL::AlbanyTraits::Jacobian, Traits>::
postRegistrationSetup(typename Traits::SetupData d,
                      PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(rho,fm);
  this->utils.setFieldData(rhoGrad,fm);
  this->utils.setFieldData(rhoDot,fm);
  this->utils.setFieldData(wBF,fm);
  this->utils.setFieldData(wGradBF,fm);
  this->utils.setFieldData(coordVec,fm);

  this->utils.setFieldData(Residual,fm);
}

template<typename Traits>
typename XScalarAdvectionResid<PHAL::AlbanyTraits::Jacobian,Traits>::ScalarT&
XScalarAdvectionResid<PHAL::AlbanyTraits::Jacobian,Traits>::getValue(const std::string &n)
{
  return Re;
}



template<typename Traits>
void XScalarAdvectionResid<PHAL::AlbanyTraits::Jacobian, Traits>::
evaluateFields(typename Traits::EvalData workset)
{
  std::vector<ScalarT> vel(numLevels);
  for (std::size_t level=0; level < numLevels; ++level) {
    vel[level] = (level+1)*Re;
  }

  for (std::size_t i=0; i < Residual.size(); ++i) Residual(i)=0.0;

  for (std::size_t cell=0; cell < workset.numCells; ++cell) {
    for (std::size_t qp=0; qp < numQPs; ++qp) {

      for (std::size_t node=0; node < numNodes; ++node) {
        for (std::size_t level=0; level < numLevels; ++level) {
          // Transient Term
          Residual(cell,node,level) += rhoDot(cell,qp,level)*wBF(cell,node,qp);
          // Advection Term
          for (std::size_t j=0; j < numDims; ++j) {
              Residual(cell,node,level) += vel[level]*rhoGrad(cell,qp,level,j)*wBF(cell,node,qp);
          }
        }
      }
    }
  }
}


}