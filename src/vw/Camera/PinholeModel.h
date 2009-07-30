// __BEGIN_LICENSE__
// Copyright (C) 2006, 2007 United States Government as represented by
// the Administrator of the National Aeronautics and Space Administration.
// All Rights Reserved.
// __END_LICENSE__


/// \file PinholeModel.h
/// 
/// This file contains the pinhole camera model.
///
#ifndef __VW_CAMERAMODEL_PINHOLE_H__
#define __VW_CAMERAMODEL_PINHOLE_H__

#include <vw/Math/Quaternion.h>
#include <vw/Camera/CameraModel.h>
#include <vw/Camera/LensDistortion.h>

#include <iostream>
#include <fstream>

namespace vw { 
namespace camera {

  /// This is a simple "generic" pinhole camera model.
  ///
  /// To specify the EXTRINSIC paramters of the camera, we specify the
  /// position of the camera center in the world frame (m_camera_center)
  /// and the pose (or orientation) of the camera in world frame (m_rotation)
  /// (which is the transformation from the camera's frame to the world frame).
  /// In the default Vision Workbench camera frame, the camera's pointing vector
  /// is the +z unit vector, and the image plane is aligned such that the 
  /// positive x-pixel direction (increasing image columns) is the camera frame's
  /// +x vector, and the positive y-pixel direction (increasing image
  /// rows) is the frame's -y vector.  Note that this discrepancy in y
  /// frames is due to the fact that images stored in memory are most
  /// naturally indexed starting in the upper left hand corner.
  ///
  /// --->The user can re-define the direction of increasing x-pixel,
  ///     increasing y-pixel, and pointing vector by specifying
  ///     orthonormal vectors u,v,w. These are intended to simplify 
  ///     movement between different camera coordinate conventions,
  ///     rather than encoding the complete rotation between world
  ///     and camera coordinate frames.
  ///
  /// The INTRINSIC portion of the camera matrix is nominally stored as
  ///
  ///    [  fx   0   cx  ]
  /// K= [  0   -fy  cy  ]
  ///    [  0    0   1   ]
  ///
  /// with fx, fy the focal length of the system (in horizontal and
  /// vertical pixels), and (cx, cy) the pixel offset of the
  /// principal point of the camera on the image plane. --Note that
  /// the default v direction is <0,-1,0>, so
  /// K will be create with a POSITIVE fy term in the center; it
  /// becomes negative when multiplied with the v_direction vector).
  ///
  /// Combining both the intrinsic camera matrix K with the
  /// extrinsic matrices, (u,v,w rotation, R and C) we see that a real-world point (x,
  /// y, z), to pixel p in an image by:
  ///
  ///     [  row  ]         [ -u- ]               [ x ]
  /// p = [  col  ]  =  K * [ -v- ] * [R | -R C]  [ y ]
  ///     [   w   ]         [ -w- ]               [ z ]
  ///
  /// p is then in homogenous coordinates, so the w has to be divided
  /// out so that w=1. Here R and C are the extrinsic parameters; R and -R*C
  /// rotate and translate a vector in world coordinates into camera coordinates.
  ///
  ///
  ///  The Tsai distortion model describes radial and tangential lens distortion. See below.
  ///

  class PinholeModel : public CameraModel {
    boost::shared_ptr<LensDistortion> m_distortion_model_ptr;
    Matrix<double,3,4> m_camera_matrix;

    // Stored for easy access.
    Vector3 m_camera_center;
    Matrix<double,3,3> m_rotation;
    Matrix<double,3,3> m_intrinsics;
    Matrix<double,3,4> m_extrinsics;

    // Intrinsic parameters, in pixel units
    double m_fu, m_fv, m_cu, m_cv;

    // Vectors that define how the coordinate system of the camera
    // relate to the directions: +u (increasing image columns), +v
    // (increasing image rows), and +w (out along optical axis)
    Vector3 m_u_direction;
    Vector3 m_v_direction;
    Vector3 m_w_direction;
    
    // Cached values for pixel_to_vector
    Matrix<double,3,3> m_inv_camera_transform;

  public:
    //------------------------------------------------------------------
    // Constructors / Destructors
    //------------------------------------------------------------------
    
    /// Initialize an empty camera model.
    PinholeModel() {
      m_u_direction = Vector3(1,0,0);
      m_v_direction = Vector3(0,-1,0);
      m_w_direction = Vector3(0,0,1);

      m_fu = 1;
      m_fv = 1;
      m_cu = 0;
      m_cv = 0;

      fill(m_camera_center,0.0);
      m_rotation.set_identity();
      this->rebuild_camera_matrix();

      m_distortion_model_ptr = boost::shared_ptr<LensDistortion>(new NullLensDistortion());
      m_distortion_model_ptr->set_parent_camera_model(this); 
    }

    /// Initialize from a file on disk.
    PinholeModel(std::string const& filename) {
      read_file(filename);
    }
    
    /// Initialize the pinhole model with explicit parameters.
    /// 
    /// The user supplies the basic intrinsic camera parameters:
    ///
    /// f_u : focal length (in units of pixels) in the u direction
    /// f_v : focal length (in units of pixels) in the v direction
    /// c_u : principal point offset (in pixels) in the u direction
    /// c_v : principal point offset (in pixels) in the v direction
    ///
    /// The direction vectors define how the coordinate system of the
    /// camera relate to the directions: +u (increasing image
    /// columns), +v (increasing image rows), and +w (complete the RH
    /// coordinate system with u and v -- points into the image)
    ///
    /// If you start from a focal length in a physical unit
    /// (e.g. meters), you can find the focal length in pixels by
    /// dividing by the pixel scale (usually in meters/pixel).  
    ///
    /// Remember that the VW standard frame of reference is such that
    /// (0,0) is the upper left hand corner of the image and the v
    /// coordinates increase as you move down the image. There is an
    /// illustration in the VisionWorkbook.
    ///

    PinholeModel(Vector3 camera_center, 
                 Matrix<double,3,3> rotation,
                 double f_u, double f_v, 
                 double c_u, double c_v,
                 Vector3 u_direction,
                 Vector3 v_direction,
                 Vector3 w_direction,
                 LensDistortion const& distortion_model) : m_camera_center(camera_center),
                                                           m_rotation(rotation),
                                                           m_fu(f_u), m_fv(f_v), m_cu(c_u), m_cv(c_v), 
                                                           m_u_direction(u_direction), 
                                                           m_v_direction(v_direction), 
                                                           m_w_direction(w_direction) {
      rebuild_camera_matrix();
      
      m_distortion_model_ptr = distortion_model.copy();
      m_distortion_model_ptr->set_parent_camera_model(this);
    }

    /// Initialize the pinhole model with explicit parameters.
    /// 
    /// The user supplies the basic intrinsic camera parameters:
    ///
    /// f_u : focal length (in units of pixels) in the u direction
    /// f_v : focal length (in units of pixels) in the v direction
    /// c_u : principal point offset (in pixels) in the u direction
    /// c_v : principal point offset (in pixels) in the v direction
    ///
    /// The direction vectors defining the coordinate system of the
    /// camera are set to default values in this version of the
    /// constructor:
    ///
    ///   +u (increasing image columns)                     =  +X   [1 0 0]
    ///   +v (increasing image rows)                        =  +Y   [0 -1 0]
    ///   +w (complete the RH coordinate system with
    ///       u and v -- points into the image)             =  +Z   [0 0 1]
    ///
    /// If you start from a focal length in a physical unit
    /// (e.g. meters), you can find the focal length in pixels by
    /// dividing by the pixel scale (usually in meters/pixel).  
    ///
    /// Remember that the VW standard frame of reference is such that
    /// (0,0) is the upper left hand corner of the image and the v
    /// coordinates increase as you move down the image.
    ///
    PinholeModel(Vector3 camera_center, 
                 Matrix<double,3,3> rotation,
                 double f_u, double f_v, 
                 double c_u, double c_v,
                 LensDistortion const& distortion_model) : m_camera_center(camera_center),
                                                           m_rotation(rotation),
                                                           m_fu(f_u), m_fv(f_v), m_cu(c_u), m_cv(c_v) {
      m_u_direction = Vector3(1,0,0);
      m_v_direction = Vector3(0,-1,0);
      m_w_direction = Vector3(0,0,1);

      rebuild_camera_matrix();

      m_distortion_model_ptr = distortion_model.copy();
      m_distortion_model_ptr->set_parent_camera_model(this);
    }


    /// Construct a basic pinhole model with no lens distortion
    PinholeModel(Vector3 camera_center, 
                 Matrix<double,3,3> rotation,
                 double f_u, double f_v, 
                 double c_u, double c_v) : m_camera_center(camera_center),
                                           m_rotation(rotation),
                                           m_fu(f_u), m_fv(f_v), m_cu(c_u), m_cv(c_v) {
      m_u_direction = Vector3(1,0,0);
      m_v_direction = Vector3(0,-1,0);
      m_w_direction = Vector3(0,0,1);

      rebuild_camera_matrix();
      m_distortion_model_ptr = boost::shared_ptr<LensDistortion>(new NullLensDistortion());
      m_distortion_model_ptr->set_parent_camera_model(this);
    }

    virtual std::string type() const { return "Pinhole"; }
    virtual ~PinholeModel() {}

    /// Read a pinhole model from a file on disk.
    void read_file(std::string const& filename);

    /// Write the parameters of a PinholeModel to disk.
    /// By convention, filename should end with ".tsai"
    void write_file(std::string const& filename) const;



    //------------------------------------------------------------------
    // Methods
    //------------------------------------------------------------------


    //  Computes the image of the point 'point' in 3D space on the
    //  image plane.  Returns a pixel location (col, row) where the
    //  point appears in the image. 
    virtual Vector2 point_to_pixel(Vector3 const& point) const {
      
      //  Multiply the pixel location by the camera matrix.
      double denominator = m_camera_matrix(2,0)*point(0) + m_camera_matrix(2,1)*point(1) + m_camera_matrix(2,2)*point(2) + m_camera_matrix(2,3);
      Vector2 pixel = Vector2( (m_camera_matrix(0,0)*point(0) + m_camera_matrix(0,1)*point(1) + m_camera_matrix(0,2)*point(2) + m_camera_matrix(0,3)) / denominator,
                               (m_camera_matrix(1,0)*point(0) + m_camera_matrix(1,1)*point(1) + m_camera_matrix(1,2)*point(2) + m_camera_matrix(1,3)) / denominator);
      
      //  Apply the lens distortion model
      return m_distortion_model_ptr->get_distorted_coordinates(pixel);
    }

    // Is a valid projection of point is possible?
    // This is equal to: Is the point in front of the camera (z > 0)
    // after extinsic transformation?
    virtual bool projection_valid(Vector3 const& point) const {
      // z coordinate after extrinsic transformation
      double z = m_extrinsics(2, 0)*point(0) + m_extrinsics(2, 1)*point(1) + m_extrinsics(2, 2)*point(2) + m_extrinsics(2,3);
      return z > 0;
    }
    

    // Returns a (normalized) pointing vector from the camera center
    //  through the position of the pixel 'pix' on the image plane.
    virtual Vector3 pixel_to_vector (Vector2 const& pix) const {

      // Apply the inverse lens distortion model
      Vector2 undistorted_pix = m_distortion_model_ptr->get_undistorted_coordinates(pix);
      
      // Compute the direction of the ray emanating from the camera center.
      Vector3 p(0,0,1);
      subvector(p,0,2) = undistorted_pix;
      return normalize( m_inv_camera_transform * p);
    }

    
    virtual Vector3 camera_center(Vector2 const& /*pix*/ = Vector2() ) const { return m_camera_center; };
    void set_camera_center(Vector3 const& position) { m_camera_center = position; rebuild_camera_matrix(); }

    //  Pose is a rotation which moves a vector in camera coordinates into world coordinates.
    virtual Quaternion<double> camera_pose(Vector2 const& /*pix*/ = Vector2() ) const { return Quaternion<double>(m_rotation); }
    void set_camera_pose(Quaternion<double> const& pose) { m_rotation = pose.rotation_matrix(); rebuild_camera_matrix(); }
    void set_camera_pose(Matrix<double,3,3> const& pose) { m_rotation = pose; rebuild_camera_matrix(); }


    //  u_direction, v_direction, and w_direction define how the coordinate
    //  system of the camera relate to the directions in the image:
    //  +u (increasing image columns),
    //  +v (increasing image rows), and
    //  +w (pointing away from the focal point in the direction of the imaged object).
    //
    //  All three vectors must be of unit length.
    
    void coordinate_frame(Vector3 &u_vec, Vector3 &v_vec, Vector3 &w_vec) const {
      u_vec = m_u_direction;
      v_vec = m_v_direction;
      w_vec = m_w_direction;
    }

    void set_coordinate_frame(Vector3 u_vec, Vector3 v_vec, Vector3 w_vec) {
      m_u_direction = u_vec;
      m_v_direction = v_vec;
      m_w_direction = w_vec;

      rebuild_camera_matrix();
    }

    // Redudant...
    Vector3 coordinate_frame_u_direction() const { return m_u_direction; }
    Vector3 coordinate_frame_v_direction() const { return m_v_direction; }
    Vector3 coordinate_frame_w_direction() const { return m_w_direction; }

    boost::shared_ptr<LensDistortion> lens_distortion() const { return m_distortion_model_ptr; };
    void set_lens_distortion(LensDistortion const& distortion) {
      m_distortion_model_ptr = distortion.copy();
      m_distortion_model_ptr->set_parent_camera_model(this);
    }

    //  f_u and f_v :  focal length in horiz and vert. pixel units
    //  c_u and c_v :  principal point in pixel units
    void intrinsic_parameters(double& f_u, double& f_v, double& c_u, double& c_v) const { 
      f_u = m_fu;  f_v = m_fv;  c_u = m_cu;  c_v = m_cv;
    }

    void set_intrinsic_parameters(double f_u, double f_v, double c_u, double c_v){
      m_fu = f_u;  m_fv = f_v;  m_cu = c_u;  m_cv = c_v;
      rebuild_camera_matrix();
    }

  private:
    /// This must be called whenever camera parameters are modified.
    void rebuild_camera_matrix() {
      
      /// The intrinsic portion of the camera matrix is stored as
      ///
      ///    [  fx   0   cx  ]
      /// K= [  0    fy  cy  ]
      ///    [  0    0   1   ]
      ///
      /// with fx, fy the focal length of the system (in horizontal and
      /// vertical pixels), and (cx, cy) the pixel coordinates of the
      /// central pixel (the principal point on the image plane).
      
      m_intrinsics(0,0) = m_fu;
      m_intrinsics(0,1) = 0;
      m_intrinsics(0,2) = m_cu;
      m_intrinsics(1,0) = 0;
      m_intrinsics(1,1) = m_fv;
      m_intrinsics(1,2) = m_cv;
      m_intrinsics(2,0) = 0;
      m_intrinsics(2,1) = 0;
      m_intrinsics(2,2) = 1;
      
      // The extrinsics are normally built as the matrix:  [ R | -R*C ].
      // To allow for user-specified coordinate frames, the
      // extrinsics are now build to include the u,v,w rotation
      //
      //               | u_0  u_1  u_2  |  
      //     Extr. =   | v_0  v_1  v_2  | * [ R | -R*C]
      //               | w_0  w_1  w_2  |
      //
      // The vectors u,v, and w must be orthonormal.
      
      /*   check for orthonormality of u,v,w              */
      VW_LINE_ASSERT( dot_prod(m_u_direction, m_v_direction) == 0 );
      VW_LINE_ASSERT( dot_prod(m_u_direction, m_w_direction) == 0 );
      VW_LINE_ASSERT( dot_prod(m_v_direction, m_w_direction) == 0 );
      VW_LINE_ASSERT( fabs( norm_2(m_u_direction) - 1 ) < 0.001 );
      VW_LINE_ASSERT( fabs( norm_2(m_v_direction) - 1 ) < 0.001 );
      VW_LINE_ASSERT( fabs( norm_2(m_w_direction) - 1 ) < 0.001 );

      Matrix<double,3,3> uvwRotation;

      select_row(uvwRotation,0) = m_u_direction;
      select_row(uvwRotation,1) = m_v_direction;
      select_row(uvwRotation,2) = m_w_direction;
      
      Matrix<double,3,3> m_rotation_inverse = transpose(m_rotation);
      submatrix(m_extrinsics,0,0,3,3) = uvwRotation * m_rotation_inverse;
      select_col(m_extrinsics,3) = uvwRotation * -m_rotation_inverse * m_camera_center;
      
      m_camera_matrix = m_intrinsics * m_extrinsics;
      m_inv_camera_transform = inverse(uvwRotation*m_rotation_inverse) * inverse(m_intrinsics);
    }

  };

  /// TSAI Lens Distortion Model
  /// 
  /// For a given set of observed (distorted) pixel coordinates, return the 
  /// location where the pixel would have appeared if there were no lens distortion.
  /// 
  /// The equations which produce these are:
  /// (u, v) = undistorted coordinates
  /// (u', v') = observed (distorted) coordinates
  /// (x, y) = object coordinates of projected point
  /// r2 = x * x + y * y   -- square of distance from object to primary vector
  /// k1, k2 are radial distortion parameters; p1, p2 are tangential distortion
  /// parameters. principal point is at (cx, cy).
  ///
  /// u' = u + (u - cx) * (k1 * r2 + k2 * r4 + 2 * p1 * y + p2 * (r2/x + 2x))
  /// v' = v + (v - cy) * (k1 * r2 + k2 * r4 + 2 * p2 * x + p1 * (r2/y + 2y))
  ///
  /// k1 is distortion[0], k2 is distortion[1],  p1 is distortion[2], p2 is distortion[3]
  ///
  /// References: Roger Tsai, A Versatile Camera Calibration Technique for a High-Accuracy 3D
  /// Machine Vision Metrology Using Off-the-shelf TV Cameras and Lenses

  class TsaiLensDistortion : public LensDistortionBase<TsaiLensDistortion, PinholeModel> {
    Vector4 m_distortion;
  public:
    TsaiLensDistortion(Vector4 params) : m_distortion(params) {
      // for debugging:
      /*
      std::cout << "k1 = " << m_distortion[0] << "\n";
      std::cout << "k2 = " << m_distortion[1] << "\n";
      std::cout << "p1 = " << m_distortion[2] << "\n";
      std::cout << "p2 = " << m_distortion[3] << "\n";
      */
    }
    virtual ~TsaiLensDistortion() {}

    Vector4 distortion_parameters() { return m_distortion; }

    //  Location where the given pixel would have appeared if there
    //  were no lens distortion.
    virtual Vector2 get_distorted_coordinates(Vector2 const& p) const {

      double fu, fv, cu, cv;
      this->camera_model().intrinsic_parameters(fu, fv, cu, cv);
      
      double du = p[0] - cu;
      double dv = p[1] - cv;

      VW_ASSERT(fu > 1e-100, MathErr() << "Tiny focal length will cause a NaN");
      VW_ASSERT(fv > 1e-100, MathErr() << "Tiny focal length will cause a NaN");
      
      double x = du / fu;  // find (x, y) using similar triangles;
      double y = dv / fv;  // assumed z=1.
      
      double x1 = m_distortion[3] / x;
      double y1 = m_distortion[2] / y;
      
      double r2 = x * x + y * y;
      
      double x3 = 2.0 * m_distortion[3] * x;
      double y3 = 2.0 * m_distortion[2] * y;
      
      double bx = r2 * (m_distortion[0] + r2 * m_distortion[1]) + x3 + y3;
      double by = bx + r2 * y1;
      bx += r2 * x1;
      
      // Prevent divide by zero at the origin or along the x and y center line
      Vector2 result(p[0] + bx * du, p[1] + by * dv);
      if (p[0] == cu)
        result[0] =  p[0];
      if (p[1] == cv)
        result[1] =  p[1];
      
      return result;
    }
   
 
    virtual void write(std::ostream & os) const {
      os << "k1 = " << m_distortion[0] << "\n";
      os << "k2 = " << m_distortion[1] << "\n";
      os << "p1 = " << m_distortion[2] << "\n";
      os << "p2 = " << m_distortion[3] << "\n";
    }

    friend std::ostream & operator<<(std::ostream & os, const TsaiLensDistortion tld){
      tld.write(os);
      return os;
    }

  };

//   /// Given two pinhole camera models, this method returns two new camera
//   /// models that have been epipolar rectified.
//   template <>
//   void epipolar(PinholeModel<NoLensDistortion> const& src_camera0, 
//                 PinholeModel<NoLensDistortion> const& src_camera1, 
//                 PinholeModel<NoLensDistortion> &dst_camera0, 
//                 PinholeModel<NoLensDistortion> &dst_camera1);

  inline PinholeModel linearize_camera(PinholeModel const& camera_model) {
    std::cout << "PinholeModel::linearize_camera \n"; 
    double fu, fv, cu, cv;
    camera_model.intrinsic_parameters(fu, fv, cu, cv);
    NullLensDistortion distortion;
    return PinholeModel(camera_model.camera_center(),
                        camera_model.camera_pose().rotation_matrix(),
                        fu, fv, cu, cv,
                        camera_model.coordinate_frame_u_direction(),
                        camera_model.coordinate_frame_v_direction(),
                        camera_model.coordinate_frame_w_direction(),
                        distortion);
  }

  // FIXME: also output the distortion parameters
  inline std::ostream& operator<<(std::ostream& str, PinholeModel const& model) {
    double fu, fv, cu, cv;
    model.intrinsic_parameters(fu, fv, cu, cv);

    str << "Pinhole camera: \n";
    str << "\tCamera Center: " << model.camera_center() << "\n";
    str << "\tRotation Matrix: " << model.camera_pose() << "\n";
    str << "\tIntrinsics:\n";
    str << "\t  f_u: " << fu << "    f_v: " << fv << "\n";
    str << "\t  c_u: " << cu << "    c_v: " << cv << "\n";
    str << model.lens_distortion() << "\n";

    return str;
  }

}}	// namespace vw::camera

#endif	//__CAMERAMODEL_CAHV_H__
