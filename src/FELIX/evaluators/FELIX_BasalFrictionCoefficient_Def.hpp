//*****************************************************************//
//    Albany 3.0:  Copyright 2016 Sandia Corporation               //
//    This Software is released under the BSD license detailed     //
//    in the file "license.txt" in the top-level Albany directory  //
//*****************************************************************//

#include "Teuchos_GlobalMPISession.hpp"
#include "Teuchos_TestForException.hpp"
#include "Teuchos_VerboseObject.hpp"
#include "Phalanx_DataLayout.hpp"
#include "Intrepid2_FunctionSpaceTools.hpp"
#include "Albany_Layouts.hpp"

//uncomment the following line if you want debug output to be printed to screen
#define OUTPUT_TO_SCREEN

namespace FELIX
{

template<typename EvalT, typename Traits, bool IsHydrology, bool IsStokes>
BasalFrictionCoefficient<EvalT, Traits, IsHydrology, IsStokes>::
BasalFrictionCoefficient (const Teuchos::ParameterList& p,
                          const Teuchos::RCP<Albany::Layouts>& dl) :
  beta        (p.get<std::string> ("Basal Friction Coefficient Variable Name"), dl->qp_scalar)
{
#ifdef OUTPUT_TO_SCREEN
  Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());

  int procRank = Teuchos::GlobalMPISession::getRank();
  int numProcs = Teuchos::GlobalMPISession::getNProc();
  output->setProcRankAndSize (procRank, numProcs);
  output->setOutputToRootOnly (0);
#endif

  Teuchos::ParameterList& beta_list = *p.get<Teuchos::ParameterList*>("Parameter List");

  std::string betaType = (beta_list.isParameter("Type") ? beta_list.get<std::string>("Type") : "Given Field");

  if (IsStokes)
  {
    TEUCHOS_TEST_FOR_EXCEPTION (!dl->isSideLayouts, Teuchos::Exceptions::InvalidParameter,
                                "Error! The layout structure does not appear to be that of a side set.\n");

    basalSideName = p.get<std::string>("Side Set Name");
    numQPs        = dl->qp_scalar->dimension(2);
    numNodes      = dl->node_scalar->dimension(2);
  }
  else
  {
    TEUCHOS_TEST_FOR_EXCEPTION (dl->isSideLayouts, Teuchos::Exceptions::InvalidParameter,
                                "Error! The layout structure appears to be that of a side set.\n");

    numQPs    = dl->qp_scalar->dimension(1);
    numNodes  = dl->node_scalar->dimension(1);
  }

  this->addEvaluatedField(beta);

  if (betaType == "Given Constant")
  {
#ifdef OUTPUT_TO_SCREEN
    *output << "Given constant and uniform beta, value loaded from xml input file.\n";
#endif
    beta_type = GIVEN_CONSTANT;
    beta_given_val = beta_list.get<double>("Constant Given Beta Value");
  }
  else if ((betaType == "Given Field")|| (betaType == "Exponent of Given Field"))
  {
#ifdef OUTPUT_TO_SCREEN
    *output << "Given constant beta field, loaded from mesh or file.\n";
#endif
    if (betaType == "Given Field")
      beta_type = GIVEN_FIELD;
    else
      beta_type = EXP_GIVEN_FIELD;

    beta_given_field = PHX::MDField<ParamScalarT>(p.get<std::string> ("Basal Friction Coefficient Variable Name") + " Given", dl->qp_scalar);

    this->addDependentField (beta_given_field);
  }
  else if (betaType == "Power Law")
  {
    beta_type = POWER_LAW;


#ifdef OUTPUT_TO_SCREEN
    *output << "Velocity-dependent beta (power law):\n\n"
            << "      beta = mu * N * |u|^p \n\n"
            << "  with N being the effective pressure, |u| the sliding velocity\n";
#endif

    N              = PHX::MDField<HydroScalarT>(p.get<std::string> ("Effective Pressure QP Variable Name"), dl->qp_scalar);
    u_norm         = PHX::MDField<IceScalarT>(p.get<std::string> ("Sliding Velocity QP Variable Name"), dl->qp_scalar);
    muParam        = PHX::MDField<ScalarT,Dim>("Coulomb Friction Coefficient", dl->shared_param);
    powerParam     = PHX::MDField<ScalarT,Dim>("Power Exponent", dl->shared_param);

    this->addDependentField (muParam);
    this->addDependentField (powerParam);
    this->addDependentField (u_norm);
    this->addDependentField (N);

    distributedLambda = beta_list.get<bool>("Distributed Bed Roughness",false);
    if (distributedLambda)
    {
      lambdaField = PHX::MDField<ParamScalarT>(p.get<std::string> ("Bed Roughness Variable Name"), dl->qp_scalar);
      this->addDependentField (lambdaField);
    }
    else
    {
      lambdaParam    = PHX::MDField<ScalarT,Dim>("Bed Roughness", dl->shared_param);
      this->addDependentField (lambdaParam);
    }
  }
  else if (betaType == "Regularized Coulomb")
  {
    beta_type = REGULARIZED_COULOMB;

    printedMu      = -9999.999;
    printedLambda  = -9999.999;
    printedQ       = -9999.999;
    if (beta_list.isParameter("Constant Flow Factor A"))
    {
      A = beta_list.get<double>("Constant Flow Factor A");

      // A*N^{1/q} is dimensionally correct only for q=1/3. To fix this, we modify A
      // so that the formula becomes (A_mod*N)^{1/q}. This means that A_mod = A^{1/3}
      //A = std::cbrt(A);
    }
    else
    {
      TEUCHOS_TEST_FOR_EXCEPTION (true, std::logic_error, "Error! The case with variable flow factor has not been implemented yet.\n");
    }
#ifdef OUTPUT_TO_SCREEN
    *output << "Velocity-dependent beta (regularized coulomb law):\n\n"
            << "      beta = mu * N * |u|^{p-1} / [|u| + lambda*A*N^(1/p)]^p\n\n"
            << "  with N being the effective pressure, |u| the sliding velocity\n";
#endif

    N              = PHX::MDField<HydroScalarT>(p.get<std::string> ("Effective Pressure QP Variable Name"), dl->qp_scalar);
    u_norm         = PHX::MDField<IceScalarT>(p.get<std::string> ("Sliding Velocity QP Variable Name"), dl->qp_scalar);
    muParam        = PHX::MDField<ScalarT,Dim>("Coulomb Friction Coefficient", dl->shared_param);
    powerParam     = PHX::MDField<ScalarT,Dim>("Power Exponent", dl->shared_param);

    this->addDependentField (muParam);
    this->addDependentField (powerParam);
    this->addDependentField (N);
    this->addDependentField (u_norm);

    distributedLambda = beta_list.get<bool>("Distributed Bed Roughness",false);
    if (distributedLambda)
    {
      lambdaField = PHX::MDField<ParamScalarT>(p.get<std::string> ("Bed Roughness Variable Name"), dl->qp_scalar);
      this->addDependentField (lambdaField);
    }
    else
    {
      lambdaParam    = PHX::MDField<ScalarT,Dim>("Bed Roughness", dl->shared_param);
      this->addDependentField (lambdaParam);
    }
  }
  else
  {
    TEUCHOS_TEST_FOR_EXCEPTION(true, Teuchos::Exceptions::InvalidParameter,
        std::endl << "Error in FELIX::BasalFrictionCoefficient:  \"" << betaType << "\" is not a valid parameter for Beta Type\n");
  }

  auto& stereographicMapList = p.get<Teuchos::ParameterList*>("Stereographic Map");
  use_stereographic_map = stereographicMapList->get("Use Stereographic Map", false);
  if(use_stereographic_map)
  {
    coordVec = PHX::MDField<MeshScalarT>(p.get<std::string>("Coordinate Vector Variable Name"), dl->qp_coords);

    double R = stereographicMapList->get<double>("Earth Radius", 6371);
    x_0 = stereographicMapList->get<double>("X_0", 0);//-136);
    y_0 = stereographicMapList->get<double>("Y_0", 0);//-2040);
    R2 = std::pow(R,2);

    this->addDependentField(coordVec);
  }

  logParameters = beta_list.get<bool>("Use log scalar parameters",false);

  this->setName("BasalFrictionCoefficient"+PHX::typeAsString<EvalT>());
}

//**********************************************************************
template<typename EvalT, typename Traits, bool IsHydrology, bool IsStokes>
void BasalFrictionCoefficient<EvalT, Traits, IsHydrology, IsStokes>::
postRegistrationSetup (typename Traits::SetupData d,
                       PHX::FieldManager<Traits>& fm)
{
  this->utils.setFieldData(beta,fm);

  switch (beta_type)
  {
    case GIVEN_CONSTANT:
      beta.deep_copy(ScalarT(beta_given_val));
      break;
    case GIVEN_FIELD:
    case EXP_GIVEN_FIELD:
      this->utils.setFieldData(beta_given_field,fm);
      break;
    case POWER_LAW:
    case REGULARIZED_COULOMB:
      this->utils.setFieldData(muParam,fm);
      this->utils.setFieldData(powerParam,fm);
      this->utils.setFieldData(N,fm);
      this->utils.setFieldData(u_norm,fm);
      if (distributedLambda)
        this->utils.setFieldData(lambdaField,fm);
      else
        this->utils.setFieldData(lambdaParam,fm);
  }

  if (use_stereographic_map)
    this->utils.setFieldData(coordVec,fm);
}

//**********************************************************************
template<typename EvalT, typename Traits, bool IsHydrology, bool IsStokes>
void BasalFrictionCoefficient<EvalT, Traits, IsHydrology, IsStokes>::
evaluateFields (typename Traits::EvalData workset)
{
  ScalarT mu, lambda, power;

  if (beta_type==POWER_LAW || beta_type==REGULARIZED_COULOMB)
  {
    if (logParameters)
    {
      mu = std::exp(muParam(0));
      power = std::exp(powerParam(0));

      if (!distributedLambda)
        lambda = std::exp(lambdaParam(0));
    }
    else
    {
      mu = muParam(0);
      power = powerParam(0);
      if (!distributedLambda)
        lambda = lambdaParam(0);
    }
#ifdef OUTPUT_TO_SCREEN
    Teuchos::RCP<Teuchos::FancyOStream> output(Teuchos::VerboseObjectBase::getDefaultOStream());
    int procRank = Teuchos::GlobalMPISession::getRank();
    int numProcs = Teuchos::GlobalMPISession::getNProc();
    output->setProcRankAndSize (procRank, numProcs);
    output->setOutputToRootOnly (0);

    if (!distributedLambda && printedLambda!=lambda)
    {
      *output << "[Basal Friction Coefficient<" << PHX::typeAsString<EvalT>() << ">] lambda = " << lambda << "\n";
      printedLambda = lambda;
    }
    if (printedMu!=mu)
    {
      *output << "[Basal Friction Coefficient<" << PHX::typeAsString<EvalT>() << ">] mu = " << mu << "\n";
      printedMu = mu;
    }
    if (printedQ!=power)
    {
      *output << "[Basal Friction Coefficient<" << PHX::typeAsString<EvalT>() << ">]] power = " << power << "\n";
      printedQ = power;
    }
#endif

    TEUCHOS_TEST_FOR_EXCEPTION (power<0, Teuchos::Exceptions::InvalidParameter,
                                "\nError in FELIX::BasalFrictionCoefficient: 'Power Exponent' must be >= 0.\n");
    TEUCHOS_TEST_FOR_EXCEPTION (mu<0, Teuchos::Exceptions::InvalidParameter,
                                "\nError in FELIX::BasalFrictionCoefficient: 'Coulomb Friction Coefficient' must be >= 0.\n");
    TEUCHOS_TEST_FOR_EXCEPTION (!distributedLambda && lambda<0, Teuchos::Exceptions::InvalidParameter,
                                "\nError in FELIX::BasalFrictionCoefficient: \"Bed Roughness\" must be >= 0.\n");
  }

  if (IsStokes)
    evaluateFieldsSide(workset,mu,lambda,power);
  else
    evaluateFieldsCell(workset,mu,lambda,power);
}

template<typename EvalT, typename Traits, bool IsHydrology, bool IsStokes>
void BasalFrictionCoefficient<EvalT, Traits, IsHydrology, IsStokes>::
evaluateFieldsSide (typename Traits::EvalData workset, ScalarT mu, ScalarT lambda, ScalarT power)
{
  if (workset.sideSets->find(basalSideName)==workset.sideSets->end())
    return;

  const std::vector<Albany::SideStruct>& sideSet = workset.sideSets->at(basalSideName);
  for (auto const& it_side : sideSet)
  {
    // Get the local data of side and cell
    const int cell = it_side.elem_LID;
    const int side = it_side.side_local_id;

    switch (beta_type)
    {
      case GIVEN_CONSTANT:
        return;   // We can save ourself some useless iterations

      case GIVEN_FIELD:
        for (int qp=0; qp<numQPs; ++qp)
        {
          beta(cell,side,qp) = beta_given_field(cell,side,qp);
        }
        break;

      case POWER_LAW:
        for (int qp=0; qp<numQPs; ++qp)
        {
          beta(cell,side,qp) = mu * N(cell,side,qp) * std::pow (u_norm(cell,side,qp), power);
        }
        break;

      case REGULARIZED_COULOMB:
        if (distributedLambda)
          for (int qp=0; qp<numQPs; ++qp)
          {
            ScalarT q = u_norm(cell,side,qp) / ( u_norm(cell,side,qp) + lambdaField(cell,side,qp)*std::pow(A*N(cell,side,qp),1./power) );
            beta(cell,side,qp) = mu * N(cell,side,qp) * std::pow( q, power) / u_norm(cell,side,qp);
          }
        else
          for (int qp=0; qp<numQPs; ++qp)
          {
            ScalarT q = u_norm(cell,side,qp) / ( u_norm(cell,side,qp) + lambda*std::pow(A*N(cell,side,qp),1./power) );
            beta(cell,side,qp) = mu * N(cell,side,qp) * std::pow( q, power) / u_norm(cell,side,qp);
          }
        break;

      case EXP_GIVEN_FIELD:
        for (int qp=0; qp<numQPs; ++qp)
        {
          beta(cell,side,qp) = std::exp(beta_given_field(cell,side,qp));
        }
        break;
    }

    // Correct the value if we are using a stereographic map
    if (use_stereographic_map)
    {
      for (int qp=0; qp<numQPs; ++qp)
      {
        MeshScalarT x = coordVec(cell,side,qp,0) - x_0;
        MeshScalarT y = coordVec(cell,side,qp,1) - y_0;
        MeshScalarT h = 4.0*R2/(4.0*R2 + x*x + y*y);
        beta(cell,side,qp) *= h*h;
      }
    }
  }
}

template<typename EvalT, typename Traits, bool IsHydrology, bool IsStokes>
void BasalFrictionCoefficient<EvalT, Traits, IsHydrology, IsStokes>::
evaluateFieldsCell (typename Traits::EvalData workset, ScalarT mu, ScalarT lambda, ScalarT power)
{
  switch (beta_type)
  {
    case GIVEN_CONSTANT:
      break;   // We don't have anything to do

    case GIVEN_FIELD:
      for (int cell=0; cell<workset.numCells; ++cell)
        for (int qp=0; qp<numQPs; ++qp)
            beta(cell,qp) = beta_given_field(cell,qp);
      break;

    case POWER_LAW:
      for (int cell=0; cell<workset.numCells; ++cell)
        for (int qp=0; qp<numQPs; ++qp)
          beta(cell,qp) = mu * N(cell,qp) * std::pow (u_norm(cell,qp), power);
      break;

    case REGULARIZED_COULOMB:
      if (distributedLambda)
      {
        if (logParameters)
          for (int cell=0; cell<workset.numCells; ++cell)
            for (int qp=0; qp<numQPs; ++qp)
            {
              ScalarT q = u_norm(cell,qp) / ( u_norm(cell,qp) + lambdaField(cell,qp)*A*std::pow(std::exp(N(cell,qp)),3) );
              beta(cell,qp) = mu * std::exp(N(cell,qp)) * std::pow( q, power) / u_norm(cell,qp);
            }
        else
          for (int cell=0; cell<workset.numCells; ++cell)
            for (int qp=0; qp<numQPs; ++qp)
            {
              ScalarT q = u_norm(cell,qp) / ( u_norm(cell,qp) + lambdaField(cell,qp)*A*std::pow(std::max(N(cell,qp),0.0),3) );
              beta(cell,qp) = mu * std::max(N(cell,qp),0.0) * std::pow( q, power) / u_norm(cell,qp);
            }
      }
      else
      {
        if (logParameters)
          for (int cell=0; cell<workset.numCells; ++cell)
            for (int qp=0; qp<numQPs; ++qp)
            {
              ScalarT q = u_norm(cell,qp) / ( u_norm(cell,qp) + lambda*A*std::pow(std::exp(N(cell,qp)),3) );
              beta(cell,qp) = mu * std::exp(N(cell,qp)) * std::pow( q, power) / u_norm(cell,qp);
            }
        else
          for (int cell=0; cell<workset.numCells; ++cell)
            for (int qp=0; qp<numQPs; ++qp)
            {
              ScalarT q = u_norm(cell,qp) / ( u_norm(cell,qp) + lambda*A*std::pow(std::max(N(cell,qp),0.0),3) );
              beta(cell,qp) = mu * std::max(N(cell,qp),0.0) * std::pow( q, power) / u_norm(cell,qp);
            }
      }
      break;

    case EXP_GIVEN_FIELD:
      for (int cell=0; cell<workset.numCells; ++cell)
        for (int qp=0; qp<numQPs; ++qp)
        {
          beta(cell,qp) = std::exp(beta_given_field(cell,qp));
        }
      break;
  }

  // Correct the value if we are using a stereographic map
  if (use_stereographic_map)
  {
    for (int cell=0; cell<workset.numCells; ++cell)
    {
      for (int qp=0; qp<numQPs; ++qp)
      {
        MeshScalarT x = coordVec(cell,qp,0) - x_0;
        MeshScalarT y = coordVec(cell,qp,1) - y_0;
        MeshScalarT h = 4.0*R2/(4.0*R2 + x*x + y*y);
        beta(cell,qp) *= h*h;
      }
    }
  }
}

} // Namespace FELIX
