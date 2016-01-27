//*****************************************************************//
//    Albany 2.0:  Copyright 2012 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#ifndef VAN_GENUCHTEN_PERMEABILITY_HPP
#define VAN_GENUCHTEN_PERMEABILITY_HPP

#include "Phalanx_config.hpp"
#include "Phalanx_Evaluator_WithBaseImpl.hpp"
#include "Phalanx_Evaluator_Derived.hpp"
#include "Phalanx_MDField.hpp"

#include "Teuchos_ParameterList.hpp"
#include "Sacado_ParameterAccessor.hpp"
#ifdef ALBANY_STOKHOS
#include "Stokhos_KL_ExponentialRandomField.hpp"
#endif
#include "Teuchos_Array.hpp"

namespace LCM {
/** 
 * \brief Evaluates permeability based on porosity
 *  through the van Genuchten equation, either as
 *  a constant or a truncated KL expansion.The permeability
 *  is divided by viscosity.
 */

template<typename EvalT, typename Traits>
class VanGenuchtenPermeability :
  public PHX::EvaluatorWithBaseImpl<Traits>,
  public PHX::EvaluatorDerived<EvalT, Traits>,
  public Sacado::ParameterAccessor<EvalT, SPL_Traits> {
  
public:
  typedef typename EvalT::ScalarT ScalarT;
  typedef typename EvalT::MeshScalarT MeshScalarT;

  VanGenuchtenPermeability(Teuchos::ParameterList& p);
  
  void postRegistrationSetup(typename Traits::SetupData d,
			     PHX::FieldManager<Traits>& vm);
  
  void evaluateFields(typename Traits::EvalData d);
  
  ScalarT& getValue(const std::string &n);

private:

  int numQPs;
  int numDims;
  PHX::MDField<MeshScalarT,Cell,QuadPoint,Dim> coordVec;
  PHX::MDField<ScalarT,Cell,QuadPoint> vgPermeability;

  //! Is van Genuchten Permeability constant, or random field
  bool is_constant;

  //! Constant value
  ScalarT constant_value;

  //! Optional dependence on porosity

  PHX::MDField<ScalarT,Cell,QuadPoint> porosity;
  PHX::MDField<ScalarT,Cell,QuadPoint> porePressure;
  bool isPoroElastic;

  ScalarT waterUnitWeight;

#ifdef ALBANY_STOKHOS
  //! Exponential random field
  Teuchos::RCP< Stokhos::KL::ExponentialRandomField<RealType>> exp_rf_kl;
#endif

  //! Values of the random variables
  Teuchos::Array<ScalarT> rv;
};
}

#endif
