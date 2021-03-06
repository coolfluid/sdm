// Copyright (C) 2010-2013 von Karman Institute for Fluid Dynamics, Belgium
//
// This software is distributed under the terms of the
// GNU Lesser General Public License version 3 (LGPLv3).
// See doc/lgpl.txt and doc/gpl.txt for the license text.

#define BOOST_TEST_DYN_LINK
#define BOOST_TEST_MODULE "Test module for cf3::dcm transformations"

#include <boost/test/unit_test.hpp>
#include <boost/assign/list_of.hpp>
#include "cf3/common/Log.hpp"
#include "cf3/common/Core.hpp"
#include "cf3/common/Environment.hpp"
#include "cf3/common/OSystem.hpp"
#include "cf3/common/OSystemLayer.hpp"
#include "cf3/common/StringConversion.hpp"
#include "cf3/common/OptionList.hpp"

#include "cf3/math/Consts.hpp"
#include "cf3/math/MatrixTypes.hpp"
#include "cf3/math/Defs.hpp"

#include "cf3/mesh/LagrangeP1/Quad2D.hpp"

using namespace boost::assign;
using namespace cf3;
using namespace cf3::common;
using namespace cf3::mesh;

//////////////////////////////////////////////////////////////////////////////

#define CF3_CHECK_EQUAL(x1,x2)\
do {\
  Real fraction=100*math::Consts::eps(); \
  if (x2 == 0) \
    BOOST_CHECK_SMALL(x1,fraction); \
  else if (x1 == 0) \
      BOOST_CHECK_SMALL(x2,fraction); \
  else \
    BOOST_CHECK_CLOSE_FRACTION(x1,x2,fraction);\
} while(false)

//////////////////////////////////////////////////////////////////////////////

struct dcm_SD_Transformations_Fixture
{
  /// common setup for each test case
  dcm_SD_Transformations_Fixture()
  {
    m_argc = boost::unit_test::framework::master_test_suite().argc;
    m_argv = boost::unit_test::framework::master_test_suite().argv;
  }

  /// common tear-down for each test case
  ~dcm_SD_Transformations_Fixture()
  {
  }
  /// possibly common functions used on the tests below


  /// common values accessed by all tests goes here
  int    m_argc;
  char** m_argv;

};

////////////////////////////////////////////////////////////////////////////////

BOOST_FIXTURE_TEST_SUITE( dcm_solver_TestSuite, dcm_SD_Transformations_Fixture )

//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( init_mpi )
{
#ifdef test_is_mpi
  PE::Comm::instance().init(m_argc,m_argv);
#endif
  Core::instance().environment().options().set("log_level",(Uint)INFO);
}

// LESSONS TO BE LEARNED HERE:
// 1) Jacobian is TRANSPOSE of what is expected in literature!!!
// 2) plane-jacobian-normals are columns of  jacobian adjoint matrix ( = inv(J)*det(J) ) ( columns because of (1) )

//////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( jacobian_eq_half_rot )
{
  // Rotation matrix, counterclockwise with angle
  Real angle = math::Consts::pi()/10.;
  RealMatrix R(2,2);
  R << std::cos(angle) ,  -std::sin(angle),
       std::sin(angle)  ,  std::cos(angle);
  Real scaling = 1.; // 2 times larger than mapped cell
  Real translation = 1.; // 2 times larger than mapped cell

  // Create geometry nodes as rotated coordinates
  RealMatrix geometry_nodes = (RealMatrix(4,2) << -1.,-1.  , 1.,-1.  ,  1.,2.  ,  -1.,2. ).finished();
  geometry_nodes.array() *= scaling;
  geometry_nodes.array() += translation;
  geometry_nodes = (R*geometry_nodes.transpose()).transpose();
  CFinfo << "geometry_nodes = \n" << geometry_nodes << CFendl;

  // Cell in mapped coordinates with a coordinate p
  RealVector mapped_p = (RealVector(2) << 1.,0.).finished();
  RealRowVector mapped_vec = (RealVector(2) << 1.,0.).finished() / scaling;

  //   3--------------2
  //   |              |
  //   |              |
  //   |              p-----> (2,0) mapped_vec
  //   |              |
  //   |              |
  //   0--------------1

  RealMatrix jacobian = mesh::LagrangeP1::Quad2D::jacobian(mapped_p,geometry_nodes);
  CFinfo << "jacobian = \n" << jacobian << CFendl;
  CFinfo << "determinant = " << jacobian.determinant() << CFendl;
  RealMatrix T = jacobian.determinant() * jacobian.inverse();
  CFinfo << "T = \n" << T << CFendl;
  CFinfo << "T.determinant = " << T.determinant() << CFendl;
  CFinfo << "Tinv = \n" << T.inverse() << CFendl;
  CFinfo << "J/|J| = \n" << jacobian / jacobian.determinant() << CFendl;
  RealMatrix J_adj = mesh::LagrangeP1::Quad2D::jacobian_adjoint(mapped_p,geometry_nodes);
  CFinfo << "adj(J) = \n" << J_adj << CFendl;

  CFinfo << "plane_jacobian_normal[KSI] = " << mesh::LagrangeP1::Quad2D::plane_jacobian_normal(mapped_p,geometry_nodes,KSI).transpose() << CFendl;
  CFinfo << "plane_jacobian_normal[ETA] = " << mesh::LagrangeP1::Quad2D::plane_jacobian_normal(mapped_p,geometry_nodes,ETA).transpose() << CFendl;

  BOOST_CHECK( J_adj == T );
  BOOST_CHECK( J_adj.col(KSI) == mesh::LagrangeP1::Quad2D::plane_jacobian_normal(mapped_p,geometry_nodes,KSI));
  BOOST_CHECK( J_adj.col(ETA) == mesh::LagrangeP1::Quad2D::plane_jacobian_normal(mapped_p,geometry_nodes,ETA));

  // Transform vector from mapped space to physical space
  RealRowVector phys_vec(2);
  phys_vec = mapped_vec*T.inverse();
  CFinfo << "phys_vec = " << phys_vec.transpose() << CFendl;
  CFinfo << "phys_vec.norm() = " << phys_vec.norm() << CFendl;

  // Transform vector from physical space to mapped space
  mapped_vec = phys_vec*T;
  CFinfo << "local_vec = " << mapped_vec << CFendl;
  CFinfo << "local_vec.norm() = " << mapped_vec.norm() << CFendl;

}

BOOST_AUTO_TEST_CASE( test1 )
{
  // Create geometry nodes as rotated coordinates
  RealMatrix geometry_nodes = ( (RealMatrix(4,2) << -1.,-1.  , 1.,-1  ,  1.,1.  ,  -1.,1. ).finished().transpose()).transpose();
  CFinfo << "geometry_nodes = \n" << geometry_nodes << CFendl;

  // Cell in mapped coordinates with a coordinate p
  RealVector mapped_p = (RealVector(2) << 1.,0.).finished();

  //   3--------------2
  //   |              |
  //   |              |
  //   |              p-----> (1,0) mapped_vec
  //   |              |
  //   |              |
  //   0--------------1

  RealRowVector phys_vec = (RealRowVector(2) << 1.,0.).finished();
  RealRowVector mapped_vec(2);

  RealMatrix jacobian = mesh::LagrangeP1::Quad2D::jacobian(mapped_p,geometry_nodes);
  RealMatrix T = jacobian.determinant() * jacobian.inverse();

  CFinfo << "jacobian = \n" << jacobian << CFendl;
  CFinfo << "determinant = " << jacobian.determinant() << CFendl;
  CFinfo << "Jxi  = " << T.row(0) << CFendl;
  CFinfo << "Jeta = " << T.row(1) << CFendl;

  mapped_vec = phys_vec * T;

  CFinfo << "phys_vec = " << phys_vec << CFendl;
  CFinfo << "mapped_vec = " << mapped_vec << CFendl;
}

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( test2 )
{
  // Create geometry nodes as rotated coordinates
  RealMatrix geometry_nodes = 2.*( (RealMatrix(4,2) <<  1.,-1  ,  1.,1.  ,  -1.,1., -1.,-1.  ).finished().transpose()).transpose();
  CFinfo << "geometry_nodes = \n" << geometry_nodes << CFendl;

  // Cell in mapped coordinates with a coordinate p
  RealVector mapped_p = (RealVector(2) << 0.,-1.).finished();


  //   2--------------1
  //   |              |
  //   |              |
  //   |              p-----> mapped_n
  //   |              |
  //   |              |
  //   3--------------0

  RealMatrix jacobian = mesh::LagrangeP1::Quad2D::jacobian(mapped_p,geometry_nodes);
  RealMatrix T = jacobian.determinant() * jacobian.inverse();

  CFinfo << "jacobian = \n" << jacobian << CFendl;
  CFinfo << "determinant = " << jacobian.determinant() << CFendl;
  RealRowVector Jxi = T.row(0);
  RealRowVector Jeta = T.row(1);
  CFinfo << "Jxi  = " << Jxi << CFendl;
  CFinfo << "Jeta = " << Jeta << CFendl;



  Real c0 = 1.4;
  RealRowVector u0 = (RealVector(2) << 0.5, 0.).finished();
  RealRowVector n  = (RealRowVector(2) << 1.,0.).finished();
  RealRowVector s  = (RealRowVector(2) << n[1],-n[0]).finished();

  RealRowVector mapped_u0(2);
  RealRowVector mapped_n(2);
  RealRowVector mapped_s(2);


  mapped_u0 = u0 * T;
  mapped_n  = n  * T;
  mapped_s  = s  * T;

  Real u0n = mapped_u0.dot(mapped_n);
  Real u0s = mapped_u0.dot(mapped_s);
  Real c0n = mapped_n.dot(mapped_n*c0);

  CFinfo << "n  = " << n << CFendl;
  CFinfo << "s  = " << s << CFendl;

  CFinfo << "mapped_u0 = " << mapped_u0 << CFendl;
  CFinfo << "mapped_n  = " << mapped_n << CFendl;
  CFinfo << "mapped_s  = " << mapped_s << CFendl;
  CFinfo << "c0n  = " << c0n << CFendl;
  CFinfo << "u0n  = " << u0n << CFendl;
  CFinfo << "u0s  = " << u0s << CFendl;


  RealRowVector mapped_unit_ksi = (RealRowVector(2) << 1, 0).finished();
  RealRowVector mapped_unit_eta = (RealRowVector(2) << 0, 1).finished();

  Real nksi = mapped_n.dot(mapped_unit_ksi);
  Real sksi = mapped_s.dot(mapped_unit_ksi);
  Real neta = mapped_n.dot(mapped_unit_eta);
  Real seta = mapped_s.dot(mapped_unit_eta);

  CFinfo << "nksi = " << nksi << CFendl;
  CFinfo << "neta = " << neta << CFendl;
  CFinfo << "sksi = " << sksi << CFendl;
  CFinfo << "seta = " << seta << CFendl;

}

////////////////////////////////////////////////////////////////////////////////

BOOST_AUTO_TEST_CASE( finalize_mpi )
{
#ifdef test_is_mpi
  PE::Comm::instance().finalize();
#endif
}

///////////////////////////////////////////////////////////////////////////////



BOOST_AUTO_TEST_SUITE_END()

////////////////////////////////////////////////////////////////////////////////
