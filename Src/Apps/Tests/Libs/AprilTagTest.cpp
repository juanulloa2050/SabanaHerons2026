/**
 * @file AprilTagTest.cpp
 *
 * Link test for the vendored apriltag library (Util/apriltag): creates and destroys
 * the tag36h11 family and a detector.
 */

#include <apriltag/apriltag.h>
#include <apriltag/tag36h11.h>

#include <gtest/gtest.h>

GTEST_TEST(AprilTag, Tag36h11FamilyCreateDestroy)
{
  apriltag_family_t* family = tag36h11_create();
  ASSERT_NE(nullptr, family);
  EXPECT_EQ(587u, family->ncodes);
  EXPECT_EQ(36, family->nbits);

  apriltag_detector_t* detector = apriltag_detector_create();
  ASSERT_NE(nullptr, detector);
  apriltag_detector_add_family(detector, family);

  apriltag_detector_destroy(detector);
  tag36h11_destroy(family);
}
