// ======================================================================== //
// Copyright 2009-2018 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#ifdef _WIN32
#  define RTC_API extern "C" __declspec(dllexport)
#else
#  define RTC_API extern "C" __attribute__ ((visibility ("default")))
#endif

#include "default.h"
#include "device.h"
#include "scene.h"
#include "context.h"
#include "../../include/embree3/rtcore_ray.h"

/********************************************** MY EDITS ****************************************************/
#include "../../kernels/bvh/bvh.h"
#include "../../kernels/geometry/trianglev.h"
#include "accel.h"
#include <queue>
#include <iostream>
#include <fstream>
#include "scene_instance.h"
#include "../../tutorials/common/tutorial/tutorial_device.h"
#include "../geometry/triangle.h"
#include "../geometry/curveNv.h"
#include "../subdiv/bezier_curve.h"

#define GEN_FILES
/************************************************************************************************************/


namespace embree
{  
  /* mutex to make API thread safe */
  static MutexSys g_mutex;

  RTC_API RTCDevice rtcNewDevice(const char* config)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewDevice);
    Lock<MutexSys> lock(g_mutex);
    Device* device = new Device(config);
    return (RTCDevice) device->refInc();
    RTC_CATCH_END(nullptr);
    return (RTCDevice) nullptr;
  }

  RTC_API void rtcRetainDevice(RTCDevice hdevice) 
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainDevice);
    RTC_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    device->refInc();
    RTC_CATCH_END(nullptr);
  }
  
  RTC_API void rtcReleaseDevice(RTCDevice hdevice) 
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseDevice);
    RTC_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    device->refDec();
    RTC_CATCH_END(nullptr);
  }
  
  RTC_API ssize_t rtcGetDeviceProperty(RTCDevice hdevice, RTCDeviceProperty prop)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetDeviceProperty);
    RTC_VERIFY_HANDLE(hdevice);
    Lock<MutexSys> lock(g_mutex);
    return device->getProperty(prop);
    RTC_CATCH_END(device);
    return 0;
  }

  RTC_API void rtcSetDeviceProperty(RTCDevice hdevice, const RTCDeviceProperty prop, ssize_t val)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceProperty);
    const bool internal_prop = (size_t)prop >= 1000000 && (size_t)prop < 1000004;
    if (!internal_prop) RTC_VERIFY_HANDLE(hdevice); // allow NULL device for special internal settings
    Lock<MutexSys> lock(g_mutex);
    device->setProperty(prop,val);
    RTC_CATCH_END(device);
  }

  RTC_API RTCError rtcGetDeviceError(RTCDevice hdevice)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetDeviceError);
    if (device == nullptr) return Device::getThreadErrorCode();
    else                   return device->getDeviceErrorCode();
    RTC_CATCH_END(device);
    return RTC_ERROR_UNKNOWN;
  }

  RTC_API void rtcSetDeviceErrorFunction(RTCDevice hdevice, RTCErrorFunction error, void* userPtr)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceErrorFunction);
    RTC_VERIFY_HANDLE(hdevice);
    device->setErrorFunction(error, userPtr);
    RTC_CATCH_END(device);
  }

  RTC_API void rtcSetDeviceMemoryMonitorFunction(RTCDevice hdevice, RTCMemoryMonitorFunction memoryMonitor, void* userPtr)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetDeviceMemoryMonitorFunction);
    device->setMemoryMonitorFunction(memoryMonitor, userPtr);
    RTC_CATCH_END(device);
  }

  RTC_API RTCBuffer rtcNewBuffer(RTCDevice hdevice, size_t byteSize)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewBuffer);
    RTC_VERIFY_HANDLE(hdevice);
    Buffer* buffer = new Buffer((Device*)hdevice, byteSize);
    return (RTCBuffer)buffer->refInc();
    RTC_CATCH_END((Device*)hdevice);
    return nullptr;
  }

  RTC_API RTCBuffer rtcNewSharedBuffer(RTCDevice hdevice, void* ptr, size_t byteSize)
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewSharedBuffer);
    RTC_VERIFY_HANDLE(hdevice);
    Buffer* buffer = new Buffer((Device*)hdevice, byteSize, ptr);
    return (RTCBuffer)buffer->refInc();
    RTC_CATCH_END((Device*)hdevice);
    return nullptr;
  }

  RTC_API void* rtcGetBufferData(RTCBuffer hbuffer)
  {
    Buffer* buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetBufferData);
    RTC_VERIFY_HANDLE(hbuffer);
    return buffer->data();
    RTC_CATCH_END2(buffer);
    return nullptr;
  }

  RTC_API void rtcRetainBuffer(RTCBuffer hbuffer)
  {
    Buffer* buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainBuffer);
    RTC_VERIFY_HANDLE(hbuffer);
    buffer->refInc();
    RTC_CATCH_END2(buffer);
  }
  
  RTC_API void rtcReleaseBuffer(RTCBuffer hbuffer)
  {
    Buffer* buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseBuffer);
    RTC_VERIFY_HANDLE(hbuffer);
    buffer->refDec();
    RTC_CATCH_END2(buffer);
  }

  RTC_API RTCScene rtcNewScene (RTCDevice hdevice) 
  {
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewScene);
    RTC_VERIFY_HANDLE(hdevice);
    Scene* scene = new Scene((Device*)hdevice);
    return (RTCScene) scene->refInc();
    RTC_CATCH_END((Device*)hdevice);
    return nullptr;
  }

  RTC_API void rtcSetSceneProgressMonitorFunction(RTCScene hscene, RTCProgressMonitorFunction progress, void* ptr) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSceneProgressMonitorFunction);
    RTC_VERIFY_HANDLE(hscene);
    Lock<MutexSys> lock(g_mutex);
    scene->setProgressMonitorFunction(progress,ptr);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcSetSceneBuildQuality (RTCScene hscene, RTCBuildQuality quality) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSceneBuildQuality);
    RTC_VERIFY_HANDLE(hscene);
    if (quality != RTC_BUILD_QUALITY_LOW &&
        quality != RTC_BUILD_QUALITY_MEDIUM &&
        quality != RTC_BUILD_QUALITY_HIGH)
      throw std::runtime_error("invalid build quality");
    scene->setBuildQuality(quality);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcSetSceneFlags (RTCScene hscene, RTCSceneFlags flags) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSceneFlags);
    RTC_VERIFY_HANDLE(hscene);
    scene->setSceneFlags(flags);
    RTC_CATCH_END2(scene);
  }

  RTC_API RTCSceneFlags rtcGetSceneFlags(RTCScene hscene)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneFlags);
    RTC_VERIFY_HANDLE(hscene);
    return scene->getSceneFlags();
    RTC_CATCH_END2(scene);
    return RTC_SCENE_FLAG_NONE;
  }
  
  RTC_API void rtcCommitScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcCommitScene);
    RTC_VERIFY_HANDLE(hscene);
    scene->commit(false);

	/**********************************  MY EDITS  **********************************************/
#ifdef GEN_FILES
	// bvh pointers to point to the bvh objext. bvh4 points to a 4-way bvh, bvh8 points to an 8-way bvh.
	BVH4* bvh4 = nullptr;
	BVH8* bvh8 = nullptr;

	/* aligned node pointers point to inner nodes within the bvh. An inner node is any bvh node that is not a leaf or
	 * root. We are using aligned bvh's so we will use AlighnedNode pointers.
	 */
	BVH4::AlignedNode* n4 = nullptr;
	BVH8::AlignedNode* n8 = nullptr;

	// nodeQueue holds the node references for inner and leaf nodes.
	std::queue<BVH4::NodeRef> nodeQueue4;
	std::queue<BVH8::NodeRef> nodeQueue8;

	// idQueue holds the node ids as the bvh is traversed so that correct order is maintained when creating the bvh file
	std::queue<unsigned long long> idQueue;

	/* boundsQueue holds the leaf node bounds (of type BBox3fa) so that they can be referenced later in the bvh traversal
	 * when calculating the leaf node's primitive coordinates.
	 */
	std::queue<BBox3fa> boundsQueue;

	/* create binary files for the bvh structure.
	 * Structure of bvhbin file is as follows:
	 * 	<64-bit node id><8-bit node type (1 for inner node, 2 for leaf node)><write 8-bit number of leaf children (primitives)>
	 * 	<6 32-bit float coordinates for the leaf node (writes most significant float byte first)>
	 */
	std::ofstream bvhbin("../generated_files/bvh.bin", std::ios::out | std::ios::binary);
	  
	/* create binary files for the primitives within the bvh.
	 * Structure of bvhbin file is as follows:
	 * 	<64-bit node id><8-bit number of primitives in leaf node><32-bit geometry id><32-bit primitive id>
	 * 	<9 32-bit float coordinates for the leaf node (writes most significant float byte first)>
	 */
	std::ofstream primbin("../generated_files/prim.bin", std::ios::out | std::ios::binary);

	/* If txt files are desired instead, uncomment the two lines below and comment out the bin file creation lines. */
	std::ofstream bvhtxt("../generated_files/bvh.txt");
	std::ofstream primtxt("../generated_files/prim.txt");
    /**/

	// NOTE: the current code only supports scenes with only triangles (for now)
	if (bvhbin.is_open() && primbin.is_open()) {
		// return acceleration structure pointer from scene object.
		AccelData* accel = ((Accel*)scene)->intersectors.ptr;

		//=================================================BVH4 CODE==========================================
		// check if acceleration structure is a 4-way bvh
		if (accel->type == AccelData::TY_BVH4) {
			// assign pointer to bvh object
			bvh4 = (BVH4*)accel;
			
			// assign pointer to bvh object
			BVH4::NodeRef node = bvh4->root;
			
			// Print that the scene bvh is a 4-way bvh
			std::cout << "4-WAY BVH" << std::endl;

			// initialize queue with root node and root id
			nodeQueue4.push(node);
			idQueue.push(0);

			// loop until entire bvh structure is search
			while (!(nodeQueue4.empty())) {
				// point tempNode to next node ref (can be inner or leaf node)
				BVH4::NodeRef tempNode = nodeQueue4.front();
				
				// remove node from nodeQueue
				nodeQueue4.pop();

				// assign tempID to the correct node ID corrisponding to tempNode
				const unsigned long long tempID = idQueue.front();
				
				// remove node ID from idQueue
				idQueue.pop();

				/**********************************************************************************************
				*	check if tempNode is a leaf node by calling isLeaf() and type() functions.
				*
				*	isLeaf() will return:
				*		-	8 if the node is a leaf node
				*		NOTE: It was also return 8 if the node is empty, which is why type() must also be called.
				*		You could just use type() to check for leaf node, but why not be safe.
				*	type() will return:
				*		-	9-15 if it's a leaf node (each number representing one of 8 leaf nodes belonging to
				*			an inner node)
				**********************************************************************************************/
				if ((tempNode.type() > 8 && tempNode.type() < 16) && tempNode.isLeaf() > 0) {	//leaf node
					// used to hold leaf number with respect to the other 8 leaves assigned to an inner node
					size_t num;

					// create bounding box objext
					BBox3fa b;

					// get bounding box coordinates for the tempNode leaf
					b = boundsQueue.front();
					
					// remove bounding box from boundsQueue
					boundsQueue.pop();

					// check the geometry type for the leaf node (current code only supports triangle4 type).
					if (strcmp(bvh4->primTy->name(), "triangle4") == 0) {	
						// cast the tempNode leaf as the appropriate primitive type (based on the geometry type)
						Triangle4* tri = (Triangle4*)tempNode.leaf(num);

						// write leaf node info to bvh file
						/***************	FOR TXT FILE	********************/
						bvhtxt << tempID << " 2 " << tri->size() << " " << b.lower.x << " " << b.lower.y << " " << b.lower.z << " "
							<< b.upper.x << " " << b.upper.y << " " << b.upper.z << "\n";
						/*******************************************************/
						
						/***************	FOR BIN FILE	********************/
						char bvharray[10];

						// write 64-bit node id
						bvharray[7] = tempID & 0xff;
						bvharray[6] = (tempID >> 8) & 0xff;
						bvharray[5] = (tempID >> 16) & 0xff;
						bvharray[4] = (tempID >> 24) & 0xff;
						bvharray[3] = (tempID >> 32) & 0xff;
						bvharray[2] = (tempID >> 40) & 0xff;
						bvharray[1] = (tempID >> 48) & 0xff;
						bvharray[0] = (tempID >> 56) & 0xff;

						// write 8-bit node type (1 for inner node, 2 for leaf node)
						bvharray[8] = 2 & 0xff;

						// write 8-bit number of leaf children (number of primitives)
						bvharray[9] = tri->size() & 0xff;
						bvhbin.write(bvharray, 10);

						// write 6 32-bit float coordinates for the leaf node
						// NOTE: writes most significant float byte first
						char* tempChar;
						tempChar = (char *)&b.lower.x;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.lower.y;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.lower.z;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.upper.x;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.upper.y;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.upper.z;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);
						/*******************************************************/


						// write primitive info to primitive file
						for (size_t j = 0; j < tri->size(); j++) {

							/***************	FOR TXT FILE	********************/
							primtxt << tempID << " " << tri->size() << " " << tri->geomID(j) << " " << tri->primID(j) <<
								" " << tri->v0.x[j] << " " << tri->v0.y[j] << " " << tri->v0.z[j] <<
								" " << tri->v0.x[j] - tri->e1.x[j] << " " << tri->v0.y[j] - tri->e1.y[j] << " " << tri->v0.z[j] - tri->e1.z[j] <<
								" " << tri->v0.x[j] + tri->e2.x[j] << " " << tri->v0.y[j] + tri->e2.y[j] << " " << tri->v0.z[j] + tri->e2.z[j] << std::endl;
							/*******************************************************/

							/***************	FOR BIN FILE	********************/
							char primarray[17];

                            // write 64-bit node id
                            primarray[7] = tempID & 0xff;
                            primarray[6] = (tempID >> 8) & 0xff;
                            primarray[5] = (tempID >> 16) & 0xff;
                            primarray[4] = (tempID >> 24) & 0xff;
                            primarray[3] = (tempID >> 32) & 0xff;
                            primarray[2] = (tempID >> 40) & 0xff;
                            primarray[1] = (tempID >> 48) & 0xff;
                            primarray[0] = (tempID >> 56) & 0xff;


							// write 8-bit number of primitives in leaf node
							primarray[8] = tri->size() & 0xff;

							// write 32-bit geometry id
							primarray[12] = tri->geomID(j) & 0xff;
							primarray[11] = (tri->geomID(j) >> 8) & 0xff;
							primarray[10] = (tri->geomID(j) >> 16) & 0xff;
							primarray[9] = (tri->geomID(j) >> 24) & 0xff;

							// write 32-bit primitive id
							primarray[16] = tri->primID(j) & 0xff;
							primarray[15] = (tri->primID(j) >> 8) & 0xff;
							primarray[14] = (tri->primID(j) >> 16) & 0xff;
							primarray[13] = (tri->primID(j) >> 24) & 0xff;
							primbin.write(primarray, 17);


							// write 9 32-bit float coordinates for the leaf node
							// NOTE: writes most significant float byte first
							float tempFloat;
							char* tempChar;
							tempChar = (char *)&tri->v0.x[j];
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempChar = (char *)&tri->v0.y[j];
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempChar = (char *)&tri->v0.z[j];
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);


							tempFloat = tri->v0.x[j] - tri->e1.x[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.y[j] - tri->e1.y[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.z[j] - tri->e1.z[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.x[j] + tri->e2.x[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.y[j] + tri->e2.y[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.z[j] + tri->e2.z[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);
							/*******************************************************/
						}
					}
					// I started trying to support flat bezier curve geometry types but I never got around to finishing it.
					/*else if (scene->geometries[0]->gtype == 4) {	// GTY_FLAT_BEZIER_CURVE
						//const PrimRef& prim = (PrimRef)tempNode.leaf(num);
						Curve4v* curve = (Curve4v*)tempNode.leaf(num);
						BezierCurve3fa* bezCurve = (BezierCurve3fa*)tempNode.leaf(num);
						QuadraticBezierCurve3fa* quadBezCurve = (QuadraticBezierCurve3fa*)tempNode.leaf(num);

						//std::cout << "\t\t\tGeomID: " << curve->geomID(0) << ", PrimID: " << curve->primID << ", Bounds: ";
						//prim->bounds() << "\n";

						std::cout << "FLAT_BEZIER_CURVE GEOMETRY" << std::endl;
					}*/
					else {	// unknown geometry type
						// this will print the unknown geometry type in the bvh
						std::cout << "ERROR: UNKNOWN GEOM TYPE: " << bvh4->primTy->name() << std::endl;
					}
				}
				// check if current node is an aligned node (inner node)
				else if (tempNode.isAlignedNode()) {
					// point aligned node pointer to tempNode
					n4 = tempNode.alignedNode();
					
					// create coordinate vectors
					vector <float> lowerX;
					vector <float> lowerY;
					vector <float> lowerZ;
					vector <float> upperX;
					vector <float> upperY;
					vector <float> upperZ;
					
					// initialize number of node children to 0
					unsigned int numChildren = 0;

					// push children bounds to their respective coordinate vectors
					for (int i = 0; i < 4; i++) {
						// an empty child node will have infinite bounds, if bounds = +-inf then break loop
						if (n4->bounds(i).lower.x == (float)pos_inf || n4->bounds(i).lower.x == (float)neg_inf) break;

						// push coordinates to their respective vectors
						lowerX.push_back(n4->bounds(i).lower.x);
						lowerY.push_back(n4->bounds(i).lower.y);
						lowerZ.push_back(n4->bounds(i).lower.z);
						upperX.push_back(n4->bounds(i).upper.x);
						upperY.push_back(n4->bounds(i).upper.y);
						upperZ.push_back(n4->bounds(i).upper.z);
						
						// increment number of children belonging to tempNode
						numChildren++;
					}

					// write inner node info to bvh file
					/***************	FOR TXT FILE	********************/
					bvhtxt << tempID << " 1 " << numChildren << " " << *std::min_element(lowerX.begin(), lowerX.end()) << " "
						<< *std::min_element(lowerY.begin(), lowerY.end()) << " " << *std::min_element(lowerZ.begin(), lowerZ.end()) << " "
						<< *std::max_element(upperX.begin(), upperX.end()) << " " << *std::max_element(upperY.begin(), upperY.end()) << " "
						<< *std::max_element(upperZ.begin(), upperZ.end()) << "\n";
					/*******************************************************/

					/***************	FOR BIN FILE	********************/
					char bvharray[10];

					// write 64-bit node id
					bvharray[7] = tempID & 0xff;
					bvharray[6] = (tempID >> 8) & 0xff;
					bvharray[5] = (tempID >> 16) & 0xff;
					bvharray[4] = (tempID >> 24) & 0xff;
					bvharray[3] = (tempID >> 32) & 0xff;
					bvharray[2] = (tempID >> 40) & 0xff;
					bvharray[1] = (tempID >> 48) & 0xff;
					bvharray[0] = (tempID >> 56) & 0xff;

					// write 8-bit node type (1 for inner node, 2 for leaf node)
					bvharray[8] = 1 & 0xff;

					// write 8-bit number of leaf children (number of primitives)
					bvharray[9] = numChildren & 0xff;
					bvhbin.write(bvharray, 10);

					// write 6 32-bit float coordinates for the leaf node
					// NOTE: writes most significant float byte first
					float tempFloat;
					char* tempChar;
					tempFloat = *std::min_element(lowerX.begin(), lowerX.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::min_element(lowerY.begin(), lowerY.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::min_element(lowerZ.begin(), lowerZ.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::max_element(upperX.begin(), upperX.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::max_element(upperY.begin(), upperY.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::max_element(upperZ.begin(), upperZ.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);
					/*******************************************************/

					// push children to queue
					for (int i = 0; i < 4; i++) {
						// double check this, but I'm pretty sure I have this here in case child node is empty in which case break for loop.
						if (n4->child(i) == 8) break;

						// push child to nodeQueue
						nodeQueue4.push(n4->child(i));

						// calculate child's node id and push to idQueue
						idQueue.push(tempID * 4 + (i + 1));

						// check if child is leaf node, if so then push leaf node bounds to boundsQueue
						if ((n4->child(i).type() > 8 && n4->child(i).type() < 16) && n4->child(i).isLeaf() > 0) {	// valid leaf node
							boundsQueue.push(n4->bounds(i));
						}
					}
				}
				else if (tempNode.type() == 8) {
					//std::cout << "\tEMPTY NODE" << "\n";
				}
				else {
					std::cout << "ERROR: UNKNOWN NODE TYPE " << tempNode.type() << "\n";
				}
			}
		}
		//===================================BVH8 ONLY=============================
		// check if acceleration structure is a 8-way bvh
		else if (accel->type == AccelData::TY_BVH8) {
			// assign pointer to bvh object
			bvh8 = (BVH8*)accel;
			
			// assign pointer to bvh root
			BVH8::NodeRef node = bvh8->root;

			// print the bvh type
			std::cout << "8-WAY BVH" << std::endl;

			// initialize node queue with root node and id queue root id
			nodeQueue8.push(node);
			idQueue.push(0);

			// loop until entire bvh structure is search
			while (!(nodeQueue8.empty())) {
				// point tempNode to next node ref (can be inner or leaf node)
				BVH8::NodeRef tempNode = nodeQueue8.front();
				
				// remove node from nodeQueue
				nodeQueue8.pop();

				// assign tempID to the correct node ID corrisponding to tempNode
				const unsigned long long tempID = idQueue.front();

				// remove node ID from idQueue
				idQueue.pop();
				
				/**********************************************************************************************
				*	check if tempNode is a leaf node calling isLeaf() and type() functions
				*	
				*	isLeaf() will return:
				*		-	8 if the node is a leaf node
				*		NOTE: It was also return 8 if the node is empty, which is why type() must also be called.
				*		You could just use type() to check for leaf node, but why not be safe.
				*	type() will return: 
				*		-	9-15 if it's a leaf node (each number representing one of 8 leaf nodes belonging to 
				*			an inner node)
				**********************************************************************************************/
				if ((tempNode.type() > 8 && tempNode.type() < 16) && tempNode.isLeaf() > 0) {
					// used to hold leaf number with respect to the other 8 leaves assigned to an inner node
					size_t num;

					// create bounding box objext
					BBox3fa b;

					// get bounding box coordinates for the tempNode leaf
					b = boundsQueue.front();
					
					// remove bounding box from boundsQueue
					boundsQueue.pop();

					// check the geometry type for the leaf node (currently only supports triangle4 types)
					if (strcmp(bvh8->primTy->name(), "triangle4") == 0) {						
						// cast the tempNode leaf as the appropriate primitive type (based on the geometry type)
						Triangle4* tri = (Triangle4*)tempNode.leaf(num);
						
						// write leaf node info to bvh file
						/***************	FOR TXT FILE	********************/
						bvhtxt << tempID << " 2 " << tri->size() << " " << b.lower.x << " " << b.lower.y << " " << b.lower.z << " "
							<< b.upper.x << " " << b.upper.y << " " << b.upper.z << "\n";
						/*******************************************************/

						/***************	FOR BIN FILE	********************/
						char bvharray[10];

						// write 64-bit node id
						bvharray[7] = tempID & 0xff;
						bvharray[6] = (tempID >> 8) & 0xff;
						bvharray[5] = (tempID >> 16) & 0xff;
						bvharray[4] = (tempID >> 24) & 0xff;
						bvharray[3] = (tempID >> 32) & 0xff;
						bvharray[2] = (tempID >> 40) & 0xff;
						bvharray[1] = (tempID >> 48) & 0xff;
						bvharray[0] = (tempID >> 56) & 0xff;

						// write 8-bit node type (1 for inner node, 2 for leaf node)
						bvharray[8] = 2 & 0xff;

						// write 8-bit number of leaf children (number of primitives)
						bvharray[9] = tri->size() & 0xff;
						bvhbin.write(bvharray, 10);

						// write 6 32-bit float coordinates for the leaf node
						// NOTE: writes most significant float byte first
						char* tempChar;
						tempChar = (char *)&b.lower.x;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.lower.y;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.lower.z;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.upper.x;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);

						tempChar = (char *)&b.upper.y;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);
						
						tempChar = (char *)&b.upper.z;
						bvhbin.write(&tempChar[3], 1);
						bvhbin.write(&tempChar[2], 1);
						bvhbin.write(&tempChar[1], 1);
						bvhbin.write(&tempChar[0], 1);
						/*******************************************************/


						// write primitive info to primitive file
						for (size_t j = 0; j < tri->size(); j++) {

							/***************	FOR TXT FILE	********************/
							primtxt << tempID << " " << tri->size() << " " << tri->geomID(j) << " " << tri->primID(j) <<
								" " << tri->v0.x[j] << " " << tri->v0.y[j] << " " << tri->v0.z[j] <<
								" " << tri->v0.x[j] - tri->e1.x[j] << " " << tri->v0.y[j] - tri->e1.y[j] << " " << tri->v0.z[j] - tri->e1.z[j] <<
								" " << tri->v0.x[j] + tri->e2.x[j] << " " << tri->v0.y[j] + tri->e2.y[j] << " " << tri->v0.z[j] + tri->e2.z[j] << std::endl;
							/*******************************************************/

							/***************	FOR BIN FILE	********************/
                            char primarray[17];

                            // write 64-bit node id
                            primarray[7] = tempID & 0xff;
                            primarray[6] = (tempID >> 8) & 0xff;
                            primarray[5] = (tempID >> 16) & 0xff;
                            primarray[4] = (tempID >> 24) & 0xff;
                            primarray[3] = (tempID >> 32) & 0xff;
                            primarray[2] = (tempID >> 40) & 0xff;
                            primarray[1] = (tempID >> 48) & 0xff;
                            primarray[0] = (tempID >> 56) & 0xff;

                            // write 8-bit number of primitives in leaf node
                            primarray[8] = tri->size() & 0xff;

                            // write 32-bit geometry id
                            primarray[12] = tri->geomID(j) & 0xff;
                            primarray[11] = (tri->geomID(j) >> 8) & 0xff;
                            primarray[10] = (tri->geomID(j) >> 16) & 0xff;
                            primarray[9] = (tri->geomID(j) >> 24) & 0xff;

                            // write 32-bit primitive id
                            primarray[16] = tri->primID(j) & 0xff;
                            primarray[15] = (tri->primID(j) >> 8) & 0xff;
                            primarray[14] = (tri->primID(j) >> 16) & 0xff;
                            primarray[13] = (tri->primID(j) >> 24) & 0xff;
                            primbin.write(primarray, 17);

							// write 6 32-bit float coordinates for the leaf node
							// NOTE: writes most significant float byte first
							float tempFloat;
							char* tempChar;
							tempChar = (char *)&tri->v0.x[j];
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempChar = (char *)&tri->v0.y[j];
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempChar = (char *)&tri->v0.z[j];
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);


							tempFloat = tri->v0.x[j] - tri->e1.x[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.y[j] - tri->e1.y[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.z[j] - tri->e1.z[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.x[j] + tri->e2.x[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.y[j] + tri->e2.y[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);

							tempFloat = tri->v0.z[j] + tri->e2.z[j];
							tempChar = (char *)&tempFloat;
							primbin.write(&tempChar[3], 1);
							primbin.write(&tempChar[2], 1);
							primbin.write(&tempChar[1], 1);
							primbin.write(&tempChar[0], 1);
							/*******************************************************/
						}
					}
					// I started trying to support flat bezier curve geometry types but I never finished
					/*else if (scene->geometries[0]->gtype == 4) {	// GTY_FLAT_BEZIER_CURVE
						//const PrimRef& prim = (PrimRef)tempNode.leaf(num);
						Curve4v* curve = (Curve4v*)tempNode.leaf(num);
						BezierCurve3fa* bezCurve = (BezierCurve3fa*)tempNode.leaf(num);
						QuadraticBezierCurve3fa* quadBezCurve = (QuadraticBezierCurve3fa*)tempNode.leaf(num);

						//std::cout << "\t\t\tGeomID: " << curve->geomID(0) << ", PrimID: " << curve->primID << ", Bounds: ";
						//prim->bounds() << "\n";

						std::cout << "ERRRO: FLAT_BEZIER_CURVE GEOMETRY" << std::endl;
					}*/
					else {	// unknown geometry type
						// this will print the unknown geometry type within the bvh
						std::cout << "ERROR: UNKNOWN GEOM TYPE: " << bvh8->primTy->name() << std::endl;
					}
				}
				// check if current node is an aligned node (inner node)
				else if (tempNode.isAlignedNode()) {

					// point aligned node pointer to tempNode
					n8 = tempNode.alignedNode();

					// create coordinate vectors
					vector <float> lowerX;
					vector <float> lowerY;
					vector <float> lowerZ;
					vector <float> upperX;
					vector <float> upperY;
					vector <float> upperZ;
					
					// init number of tempNode children to 0
					unsigned int numChildren = 0;

					// push children bounds to their respective coordinate vectors
					for (int i = 0; i < 8; i++) {
						// an empty child node will have infinite bounds, if bounds = +-inf then and break loop
						if (n8->bounds(i).lower.x == (float)pos_inf || n8->bounds(i).lower.x == (float)neg_inf) break;

						// push coordinates to their respective vectors
						lowerX.push_back(n8->bounds(i).lower.x);
						lowerY.push_back(n8->bounds(i).lower.y);
						lowerZ.push_back(n8->bounds(i).lower.z);
						upperX.push_back(n8->bounds(i).upper.x);
						upperY.push_back(n8->bounds(i).upper.y);
						upperZ.push_back(n8->bounds(i).upper.z);
						
						// increment children count for tempNode
						numChildren++;
					}

					// write inner node info to bvh file
					/***************	FOR TXT FILE	********************/
					bvhtxt << tempID << " 1 " << numChildren << " " << *std::min_element(lowerX.begin(), lowerX.end()) << " "
						<< *std::min_element(lowerY.begin(), lowerY.end()) << " " << *std::min_element(lowerZ.begin(), lowerZ.end()) << " "
						<< *std::max_element(upperX.begin(), upperX.end()) << " " << *std::max_element(upperY.begin(), upperY.end()) << " "
						<< *std::max_element(upperZ.begin(), upperZ.end()) << "\n";
					/*******************************************************/

					/***************	FOR BIN FILE	********************/
					char bvharray[10];

					// write 64-bit node id
					bvharray[7] = tempID & 0xff;
					bvharray[6] = (tempID >> 8) & 0xff;
					bvharray[5] = (tempID >> 16) & 0xff;
					bvharray[4] = (tempID >> 24) & 0xff;
					bvharray[3] = (tempID >> 32) & 0xff;
					bvharray[2] = (tempID >> 40) & 0xff;
					bvharray[1] = (tempID >> 48) & 0xff;
					bvharray[0] = (tempID >> 56) & 0xff;

					// write 8-bit node type (1 for inner node, 2 for leaf node)
					bvharray[8] = 1 & 0xff;

					// write 8-bit number of leaf children (number of primitives)
					bvharray[9] = numChildren & 0xff;
					bvhbin.write(bvharray, 10);

					// write 6 32-bit float coordinates for the leaf node
					// NOTE: writes most significant float byte first
					float tempFloat;
					char* tempChar;
					tempFloat = *std::min_element(lowerX.begin(), lowerX.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::min_element(lowerY.begin(), lowerY.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::min_element(lowerZ.begin(), lowerZ.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::max_element(upperX.begin(), upperX.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::max_element(upperY.begin(), upperY.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);

					tempFloat = *std::max_element(upperZ.begin(), upperZ.end());
					tempChar = (char *)&tempFloat;
					bvhbin.write(&tempChar[3], 1);
					bvhbin.write(&tempChar[2], 1);
					bvhbin.write(&tempChar[1], 1);
					bvhbin.write(&tempChar[0], 1);
					/*******************************************************/

					// push inner node's children to node queue, as well as their node id to the id queue
					for (int i = 0; i < 8; i++) {
						// double check this, but I'm pretty sure I have this here in case child node is empty in which case break for loop.
						if (n8->child(i) == 8) break;
						
						// push child onto nodeQueue
						nodeQueue8.push(n8->child(i));
						
						// calculate node id and push to idQueue
						idQueue.push(tempID * 8 + (i + 1));

						// check if child is a leaf node, if so push leaf node bounds to boundsQueue
						if ((n8->child(i).type() > 8 && n8->child(i).type() < 16) && n8->child(i).isLeaf() > 0) {	// valid leaf node
							boundsQueue.push(n8->bounds(i));
						}
					}
				}
				// empty node, do not process
				else if (tempNode.type() == 8) {
					//std::cout << "\tEMPTY NODE" << "\n";
				}
				// unknown node type (could be one of the other nodes defined in bvh.h)
				else {
					std::cout << "ERROR: UNKNOWN NODE TYPE " << tempNode.type() << "\n";
				}
			}
		}

		// unknown bvh structure (could be 2-level bvh instance)
		else {
			std::cout << "UNKNOWN BVH STRUCTURE" << std::endl;
		}
	}
	else {
		std::cout << "ERROR: UNABLE TO OPEN TEXT FILE";
	}
	bvhbin.close();
	primbin.close();
	bvhtxt.close();
	primtxt.close();
#endif
	/***********************************************************************************/

    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcJoinCommitScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcJoinCommitScene);
    RTC_VERIFY_HANDLE(hscene);
    scene->commit(true);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcGetSceneBounds(RTCScene hscene, RTCBounds* bounds_o)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneBounds);
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    BBox3fa bounds = scene->bounds.bounds();
    bounds_o->lower_x = bounds.lower.x;
    bounds_o->lower_y = bounds.lower.y;
    bounds_o->lower_z = bounds.lower.z;
    bounds_o->align0  = 0;
    bounds_o->upper_x = bounds.upper.x;
    bounds_o->upper_y = bounds.upper.y;
    bounds_o->upper_z = bounds.upper.z;
    bounds_o->align1  = 0;
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcGetSceneLinearBounds(RTCScene hscene, RTCLinearBounds* bounds_o)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetSceneBounds);
    RTC_VERIFY_HANDLE(hscene);
    if (bounds_o == nullptr)
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"invalid destination pointer");
    if (scene->isModified())
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    
    bounds_o->bounds0.lower_x = scene->bounds.bounds0.lower.x;
    bounds_o->bounds0.lower_y = scene->bounds.bounds0.lower.y;
    bounds_o->bounds0.lower_z = scene->bounds.bounds0.lower.z;
    bounds_o->bounds0.align0  = 0;
    bounds_o->bounds0.upper_x = scene->bounds.bounds0.upper.x;
    bounds_o->bounds0.upper_y = scene->bounds.bounds0.upper.y;
    bounds_o->bounds0.upper_z = scene->bounds.bounds0.upper.z;
    bounds_o->bounds0.align1  = 0;
    bounds_o->bounds1.lower_x = scene->bounds.bounds1.lower.x;
    bounds_o->bounds1.lower_y = scene->bounds.bounds1.lower.y;
    bounds_o->bounds1.lower_z = scene->bounds.bounds1.lower.z;
    bounds_o->bounds1.align0  = 0;
    bounds_o->bounds1.upper_x = scene->bounds.bounds1.upper.x;
    bounds_o->bounds1.upper_y = scene->bounds.bounds1.upper.y;
    bounds_o->bounds1.upper_z = scene->bounds.bounds1.upper.z;
    bounds_o->bounds1.align1  = 0;
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcIntersect1 (RTCScene hscene, RTCIntersectContext* user_context, RTCRayHit* rayhit) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect1);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rayhit) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT3(normal.travs,1,1,1);
    IntersectContext context(scene,user_context);
    scene->intersectors.intersect(*rayhit,&context);
#if defined(DEBUG)
    ((RayHit*)rayhit)->verifyHit();
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersect4 (const int* valid, RTCScene hscene, RTCIntersectContext* user_context, RTCRayHit4* rayhit) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect4);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)rayhit)   & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
#if !defined(EMBREE_RAY_PACKETS)
    Ray4* ray4 = (Ray4*) rayhit;
    for (size_t i=0; i<4; i++) {
      if (!valid[i]) continue;
      RayHit ray1; ray4->get(i,ray1);
      scene->intersectors.intersect((RTCRayHit&)ray1,&context);
      ray4->set(i,ray1);
    }
#else
    scene->intersectors.intersect4(valid,*rayhit,&context);
#endif
    
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcIntersect8 (const int* valid, RTCScene hscene, RTCIntersectContext* user_context, RTCRayHit8* rayhit) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect8);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 32 bytes");   
    if (((size_t)rayhit)   & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit not aligned to 32 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<8; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
#if !defined(EMBREE_RAY_PACKETS)
    Ray8* ray8 = (Ray8*) rayhit;
    for (size_t i=0; i<8; i++) {
      if (!valid[i]) continue;
      RayHit ray1; ray8->get(i,ray1);
      scene->intersectors.intersect((RTCRayHit&)ray1,&context);
      ray8->set(i,ray1);
    }
#else
    if (likely(scene->intersectors.intersector8))
      scene->intersectors.intersect8(valid,*rayhit,&context);
    else
      scene->device->rayStreamFilters.intersectSOA(scene,(char*)rayhit,8,1,sizeof(RTCRayHit8),&context);
#endif
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcIntersect16 (const int* valid, RTCScene hscene, RTCIntersectContext* user_context, RTCRayHit16* rayhit) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect16);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 64 bytes");   
    if (((size_t)rayhit)   & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit not aligned to 64 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<16; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(normal.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
#if !defined(EMBREE_RAY_PACKETS)
    Ray16* ray16 = (Ray16*) rayhit;
    for (size_t i=0; i<16; i++) {
      if (!valid[i]) continue;
      RayHit ray1; ray16->get(i,ray1);
      scene->intersectors.intersect((RTCRayHit&)ray1,&context);
      ray16->set(i,ray1);
    }
#else
    if (likely(scene->intersectors.intersector16))
      scene->intersectors.intersect16(valid,*rayhit,&context);
    else
      scene->device->rayStreamFilters.intersectSOA(scene,(char*)rayhit,16,1,sizeof(RTCRayHit16),&context);
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersect1M (RTCScene hscene, RTCIntersectContext* user_context, RTCRayHit* rayhit, unsigned int M, size_t byteStride) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect1M);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rayhit ) & 0x03) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for single rays */
    if (likely(M == 1)) {
      if (likely(rayhit->ray.tnear <= rayhit->ray.tfar)) 
        scene->intersectors.intersect(*rayhit,&context);
    } 

    /* codepath for streams */
    else {
      scene->device->rayStreamFilters.intersectAOS(scene,rayhit,M,byteStride,&context);   
    }
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersect1M not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersect1Mp (RTCScene hscene, RTCIntersectContext* user_context, RTCRayHit** rn, unsigned int M) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersect1Mp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rn) & 0x03) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for single rays */
    if (likely(M == 1)) {
      if (likely(rn[0]->ray.tnear <= rn[0]->ray.tfar)) 
        scene->intersectors.intersect(*rn[0],&context);
    } 

    /* codepath for streams */
    else {
      scene->device->rayStreamFilters.intersectAOP(scene,rn,M,&context);
    }
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersect1Mp not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersectNM (RTCScene hscene, RTCIntersectContext* user_context, struct RTCRayHitN* rayhit, unsigned int N, unsigned int M, size_t byteStride) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersectNM);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rayhit) & 0x03) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,N*M,N*M,N*M);
    IntersectContext context(scene,user_context);

    /* code path for single ray streams */
    if (likely(N == 1))
    {
      /* fast code path for streams of size 1 */
      if (likely(M == 1)) {
        if (likely(((RTCRayHit*)rayhit)->ray.tnear <= ((RTCRayHit*)rayhit)->ray.tfar))
          scene->intersectors.intersect(*(RTCRayHit*)rayhit,&context);
      } 
      /* normal codepath for single ray streams */
      else {
        scene->device->rayStreamFilters.intersectAOS(scene,(RTCRayHit*)rayhit,M,byteStride,&context);
      }
    }
    /* code path for ray packet streams */
    else {
      scene->device->rayStreamFilters.intersectSOA(scene,(char*)rayhit,N,M,byteStride,&context);
    }
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersectNM not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcIntersectNp (RTCScene hscene, RTCIntersectContext* user_context, const RTCRayHitNp* rayhit, unsigned int N) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcIntersectNp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)rayhit->ray.org_x ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.org_x not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.org_y ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.org_y not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.org_z ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.org_z not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.dir_x ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.dir_x not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.dir_y ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.dir_y not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.dir_z ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.dir_z not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.tnear ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.dir_x not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.tfar  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.tnear not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.time  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.time not aligned to 4 bytes");   
    if (((size_t)rayhit->ray.mask  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->ray.mask not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.Ng_x  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.Ng_x not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.Ng_y  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.Ng_y not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.Ng_z  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.Ng_z not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.u     ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.u not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.v     ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.v not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.geomID) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.geomID not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.primID) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.primID not aligned to 4 bytes");   
    if (((size_t)rayhit->hit.instID) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "rayhit->hit.instID not aligned to 4 bytes");   
#endif
    STAT3(normal.travs,N,N,N);
    IntersectContext context(scene,user_context);
    scene->device->rayStreamFilters.intersectSOP(scene,rayhit,N,&context);
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcIntersectNp not supported");
#endif
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcOccluded1 (RTCScene hscene, RTCIntersectContext* user_context, RTCRay* ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded1);
    STAT3(shadow.travs,1,1,1);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)ray) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    IntersectContext context(scene,user_context);
    scene->intersectors.occluded(*ray,&context);
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcOccluded4 (const int* valid, RTCScene hscene, RTCIntersectContext* user_context, RTCRay4* ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded4);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 16 bytes");   
    if (((size_t)ray)   & 0x0F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 16 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<4; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
#if !defined(EMBREE_RAY_PACKETS)
    RayHit4* ray4 = (RayHit4*) ray;
    for (size_t i=0; i<4; i++) {
      if (!valid[i]) continue;
      RayHit ray1; ray4->get(i,ray1);
      scene->intersectors.occluded((RTCRay&)ray1,&context);
      ray4->geomID[i] = ray1.geomID; 
    }
#else
    scene->intersectors.occluded4(valid,*ray,&context);
#endif
    
    RTC_CATCH_END2(scene);
  }
 
  RTC_API void rtcOccluded8 (const int* valid, RTCScene hscene, RTCIntersectContext* user_context, RTCRay8* ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded8);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 32 bytes");   
    if (((size_t)ray)   & 0x1F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 32 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<8; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
#if !defined(EMBREE_RAY_PACKETS)
    RayHit8* ray8 = (RayHit8*) ray;
    for (size_t i=0; i<8; i++) {
      if (!valid[i]) continue;
      RayHit ray1; ray8->get(i,ray1);
      scene->intersectors.occluded((RTCRay&)ray1,&context);
      ray8->set(i,ray1);
    }
#else
    if (likely(scene->intersectors.intersector8))
      scene->intersectors.occluded8(valid,*ray,&context);
    else
      scene->device->rayStreamFilters.occludedSOA(scene,(char*)ray,8,1,sizeof(RTCRay8),&context);
#endif

    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcOccluded16 (const int* valid, RTCScene hscene, RTCIntersectContext* user_context, RTCRay16* ray) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded16);

#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)valid) & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 64 bytes");   
    if (((size_t)ray)   & 0x3F) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 64 bytes");   
#endif
    STAT(size_t cnt=0; for (size_t i=0; i<16; i++) cnt += ((int*)valid)[i] == -1;);
    STAT3(shadow.travs,cnt,cnt,cnt);

    IntersectContext context(scene,user_context);
#if !defined(EMBREE_RAY_PACKETS)
    RayHit16* ray16 = (RayHit16*) ray;
    for (size_t i=0; i<16; i++) {
      if (!valid[i]) continue;
      RayHit ray1; ray16->get(i,ray1);
      scene->intersectors.occluded((RTCRay&)ray1,&context);
      ray16->set(i,ray1);
    }
#else
    if (likely(scene->intersectors.intersector16))
      scene->intersectors.occluded16(valid,*ray,&context);
    else
      scene->device->rayStreamFilters.occludedSOA(scene,(char*)ray,16,1,sizeof(RTCRay16),&context);
#endif

    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcOccluded1M(RTCScene hscene, RTCIntersectContext* user_context, RTCRay* ray, unsigned int M, size_t byteStride) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded1M);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)ray) & 0x03) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,M,M,M);
    IntersectContext context(scene,user_context);
    /* fast codepath for streams of size 1 */
    if (likely(M == 1)) {
      if (likely(ray->tnear <= ray->tfar)) 
        scene->intersectors.occluded (*ray,&context);
    } 
    /* codepath for normal streams */
    else {
      scene->device->rayStreamFilters.occludedAOS(scene,ray,M,byteStride,&context);
    }
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcOccluded1M not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcOccluded1Mp(RTCScene hscene, RTCIntersectContext* user_context, RTCRay** ray, unsigned int M) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccluded1Mp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)ray) & 0x03) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,M,M,M);
    IntersectContext context(scene,user_context);

    /* fast codepath for streams of size 1 */
    if (likely(M == 1)) {
      if (likely(ray[0]->tnear <= ray[0]->tfar)) 
        scene->intersectors.occluded (*ray[0],&context);
    } 
    /* codepath for normal streams */
    else {
      scene->device->rayStreamFilters.occludedAOP(scene,ray,M,&context);
    }
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcOccluded1Mp not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcOccludedNM(RTCScene hscene, RTCIntersectContext* user_context, RTCRayN* ray, unsigned int N, unsigned int M, size_t byteStride)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccludedNM);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (byteStride < sizeof(RTCRayHit)) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"byteStride too small");
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)ray) & 0x03) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "ray not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,N*M,N*N,N*N);
    IntersectContext context(scene,user_context);

    /* codepath for single rays */
    if (likely(N == 1))
    {
      /* fast path for streams of size 1 */
      if (likely(M == 1)) {
        if (likely(((RTCRay*)ray)->tnear <= ((RTCRay*)ray)->tfar))
          scene->intersectors.occluded (*(RTCRay*)ray,&context);
      } 
      /* codepath for normal ray streams */
      else {
        scene->device->rayStreamFilters.occludedAOS(scene,(RTCRay*)ray,M,byteStride,&context);
      }
    }
    /* code path for ray packet streams */
    else {
      scene->device->rayStreamFilters.occludedSOA(scene,(char*)ray,N,M,byteStride,&context);
    }
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcOccludedNM not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcOccludedNp(RTCScene hscene, RTCIntersectContext* user_context, const RTCRayNp* ray, unsigned int N)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcOccludedNp);

#if defined (EMBREE_RAY_PACKETS)
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    if (scene->isModified()) throw_RTCError(RTC_ERROR_INVALID_OPERATION,"scene got not committed");
    if (((size_t)ray->org_x ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "org_x not aligned to 4 bytes");   
    if (((size_t)ray->org_y ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "org_y not aligned to 4 bytes");   
    if (((size_t)ray->org_z ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "org_z not aligned to 4 bytes");   
    if (((size_t)ray->dir_x ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "dir_x not aligned to 4 bytes");   
    if (((size_t)ray->dir_y ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "dir_y not aligned to 4 bytes");   
    if (((size_t)ray->dir_z ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "dir_z not aligned to 4 bytes");   
    if (((size_t)ray->tnear ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "dir_x not aligned to 4 bytes");   
    if (((size_t)ray->tfar  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "tnear not aligned to 4 bytes");   
    if (((size_t)ray->time  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "time not aligned to 4 bytes");   
    if (((size_t)ray->mask  ) & 0x03 ) throw_RTCError(RTC_ERROR_INVALID_ARGUMENT, "mask not aligned to 4 bytes");   
#endif
    STAT3(shadow.travs,N,N,N);
    IntersectContext context(scene,user_context);
    scene->device->rayStreamFilters.occludedSOP(scene,ray,N,&context);
#else
    throw_RTCError(RTC_ERROR_INVALID_OPERATION,"rtcOccludedNp not supported");
#endif
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcRetainScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainScene);
    RTC_VERIFY_HANDLE(hscene);
    scene->refInc();
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcReleaseScene (RTCScene hscene) 
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseScene);
    RTC_VERIFY_HANDLE(hscene);
    scene->refDec();
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcSetGeometryInstancedScene(RTCGeometry hgeometry, RTCScene hscene)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    Ref<Scene> scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryInstancedScene);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(hscene);
    geometry->setInstancedScene(scene);
    RTC_CATCH_END2(geometry);
  }

  AffineSpace3fa loadTransform(RTCFormat format, const float* xfm)
  {
    AffineSpace3fa space = one;
    switch (format)
    {
    case RTC_FORMAT_FLOAT3X4_ROW_MAJOR:
      space = AffineSpace3fa(Vec3fa(xfm[ 0], xfm[ 4], xfm[ 8]),
                             Vec3fa(xfm[ 1], xfm[ 5], xfm[ 9]),
                             Vec3fa(xfm[ 2], xfm[ 6], xfm[10]),
                             Vec3fa(xfm[ 3], xfm[ 7], xfm[11]));
      break;

    case RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR:
      space = AffineSpace3fa(Vec3fa(xfm[ 0], xfm[ 1], xfm[ 2]),
                             Vec3fa(xfm[ 3], xfm[ 4], xfm[ 5]),
                             Vec3fa(xfm[ 6], xfm[ 7], xfm[ 8]),
                             Vec3fa(xfm[ 9], xfm[10], xfm[11]));
      break;

    case RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR:
      space = AffineSpace3fa(Vec3fa(xfm[ 0], xfm[ 1], xfm[ 2]),
                             Vec3fa(xfm[ 4], xfm[ 5], xfm[ 6]),
                             Vec3fa(xfm[ 8], xfm[ 9], xfm[10]),
                             Vec3fa(xfm[12], xfm[13], xfm[14]));
      break;

    default: 
      throw_RTCError(RTC_ERROR_INVALID_OPERATION, "invalid matrix format");
      break;
    }
    return space;
  }

  void storeTransform(const AffineSpace3fa& space, RTCFormat format, float* xfm)
  {
    switch (format)
    {
    case RTC_FORMAT_FLOAT3X4_ROW_MAJOR:
      xfm[ 0] = space.l.vx.x;  xfm[ 1] = space.l.vy.x;  xfm[ 2] = space.l.vz.x;  xfm[ 3] = space.p.x;
      xfm[ 4] = space.l.vx.y;  xfm[ 5] = space.l.vy.y;  xfm[ 6] = space.l.vz.y;  xfm[ 7] = space.p.y;
      xfm[ 8] = space.l.vx.z;  xfm[ 9] = space.l.vy.z;  xfm[10] = space.l.vz.z;  xfm[11] = space.p.z;
      break;

    case RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR:
      xfm[ 0] = space.l.vx.x;  xfm[ 1] = space.l.vx.y;  xfm[ 2] = space.l.vx.z;
      xfm[ 3] = space.l.vy.x;  xfm[ 4] = space.l.vy.y;  xfm[ 5] = space.l.vy.z;
      xfm[ 6] = space.l.vz.x;  xfm[ 7] = space.l.vz.y;  xfm[ 8] = space.l.vz.z;
      xfm[ 9] = space.p.x;     xfm[10] = space.p.y;     xfm[11] = space.p.z;
      break;

    case RTC_FORMAT_FLOAT4X4_COLUMN_MAJOR:
      xfm[ 0] = space.l.vx.x;  xfm[ 1] = space.l.vx.y;  xfm[ 2] = space.l.vx.z;  xfm[ 3] = 0.f;
      xfm[ 4] = space.l.vy.x;  xfm[ 5] = space.l.vy.y;  xfm[ 6] = space.l.vy.z;  xfm[ 7] = 0.f;
      xfm[ 8] = space.l.vz.x;  xfm[ 9] = space.l.vz.y;  xfm[10] = space.l.vz.z;  xfm[11] = 0.f;
      xfm[12] = space.p.x;     xfm[13] = space.p.y;     xfm[14] = space.p.z;     xfm[15] = 1.f;
      break;

    default:
      throw_RTCError(RTC_ERROR_INVALID_OPERATION, "invalid matrix format");
      break;
    }
  }

  RTC_API void rtcSetGeometryTransform(RTCGeometry hgeometry, unsigned int timeStep, RTCFormat format, const void* xfm)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTransform);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(xfm);
    const AffineSpace3fa transform = loadTransform(format, (const float*)xfm);
    geometry->setTransform(transform, timeStep);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcGetGeometryTransform(RTCGeometry hgeometry, float time, RTCFormat format, void* xfm)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryTransform);
    const AffineSpace3fa transform = geometry->getTransform(time);
    storeTransform(transform, format, (float*)xfm);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcFilterIntersection(const struct RTCIntersectFunctionNArguments* const args_i, const struct RTCFilterFunctionNArguments* filter_args)
  {
    IntersectFunctionNArguments* args = (IntersectFunctionNArguments*) args_i;
    args->report(args,filter_args);
  }

  RTC_API void rtcFilterOcclusion(const struct RTCOccludedFunctionNArguments* const args_i, const struct RTCFilterFunctionNArguments* filter_args)
  {
    OccludedFunctionNArguments* args = (OccludedFunctionNArguments*) args_i;
    args->report(args,filter_args);
  }
  
  RTC_API RTCGeometry rtcNewGeometry (RTCDevice hdevice, RTCGeometryType type)
  {
    Device* device = (Device*) hdevice;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcNewGeometry);
    RTC_VERIFY_HANDLE(hdevice);

    switch (type)
    {
    case RTC_GEOMETRY_TYPE_TRIANGLE:
    {
#if defined(EMBREE_GEOMETRY_TRIANGLE)
      createTriangleMeshTy createTriangleMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createTriangleMesh);
      Geometry* geom = createTriangleMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_TRIANGLE is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_QUAD:
    {
#if defined(EMBREE_GEOMETRY_QUAD)
      createQuadMeshTy createQuadMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createQuadMesh);
      Geometry* geom = createQuadMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_QUAD is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE:
      
    case RTC_GEOMETRY_TYPE_ROUND_BEZIER_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_BEZIER_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BEZIER_CURVE:
      
    case RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_BSPLINE_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BSPLINE_CURVE:

    case RTC_GEOMETRY_TYPE_ROUND_HERMITE_CURVE:
    case RTC_GEOMETRY_TYPE_FLAT_HERMITE_CURVE:
    case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_HERMITE_CURVE:
    {
#if defined(EMBREE_GEOMETRY_CURVE)
      createLineSegmentsTy createLineSegments = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createLineSegments);
      createCurvesTy createCurves = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createCurves);
      
      Geometry* geom;
      switch (type) {
      //case RTC_GEOMETRY_TYPE_ROUND_LINEAR_CURVE            : geom = createLineSegments (device,Geometry::GTY_ROUND_LINEAR_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_LINEAR_CURVE             : geom = createLineSegments (device,Geometry::GTY_FLAT_LINEAR_CURVE); break;
      //case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_LINEAR_CURVE  : geom = createLineSegments (device,Geometry::GTY_ORIENTED_LINEAR_CURVE); break;
        
      case RTC_GEOMETRY_TYPE_ROUND_BEZIER_CURVE            : geom = createCurves(device,Geometry::GTY_ROUND_BEZIER_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_BEZIER_CURVE             : geom = createCurves(device,Geometry::GTY_FLAT_BEZIER_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BEZIER_CURVE  : geom = createCurves(device,Geometry::GTY_ORIENTED_BEZIER_CURVE); break;
        
      case RTC_GEOMETRY_TYPE_ROUND_BSPLINE_CURVE           : geom = createCurves(device,Geometry::GTY_ROUND_BSPLINE_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_BSPLINE_CURVE            : geom = createCurves(device,Geometry::GTY_FLAT_BSPLINE_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_BSPLINE_CURVE : geom = createCurves(device,Geometry::GTY_ORIENTED_BSPLINE_CURVE); break;
        
      case RTC_GEOMETRY_TYPE_ROUND_HERMITE_CURVE           : geom = createCurves(device,Geometry::GTY_ROUND_HERMITE_CURVE); break;
      case RTC_GEOMETRY_TYPE_FLAT_HERMITE_CURVE            : geom = createCurves(device,Geometry::GTY_FLAT_HERMITE_CURVE); break;
      case RTC_GEOMETRY_TYPE_NORMAL_ORIENTED_HERMITE_CURVE : geom = createCurves(device,Geometry::GTY_ORIENTED_HERMITE_CURVE); break;
      default:                                    geom = nullptr; break;
      }
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_CURVE is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_SUBDIVISION:
    {
#if defined(EMBREE_GEOMETRY_SUBDIVISION)
      createSubdivMeshTy createSubdivMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX(device->enabled_cpu_features,createSubdivMesh);
      //SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createSubdivMesh); // FIXME: this does not work for some reason?
      Geometry* geom = createSubdivMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_SUBDIVISION is not supported");
#endif
    }
    
    case RTC_GEOMETRY_TYPE_USER:
    {
#if defined(EMBREE_GEOMETRY_USER)
      createUserGeometryTy createUserGeometry = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createUserGeometry);
      Geometry* geom = createUserGeometry(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_USER is not supported");
#endif
    }

    case RTC_GEOMETRY_TYPE_INSTANCE:
    {
#if defined(EMBREE_GEOMETRY_INSTANCE)
      createInstanceTy createInstance = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createInstance);
      Geometry* geom = createInstance(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_INSTANCE is not supported");
#endif
    }

    case RTC_GEOMETRY_TYPE_GRID:
    {
#if defined(EMBREE_GEOMETRY_GRID)
      createGridMeshTy createGridMesh = nullptr;
      SELECT_SYMBOL_DEFAULT_AVX_AVX2_AVX512KNL_AVX512SKX(device->enabled_cpu_features,createGridMesh);
      Geometry* geom = createGridMesh(device);
      return (RTCGeometry) geom->refInc();
#else
      throw_RTCError(RTC_ERROR_UNKNOWN,"RTC_GEOMETRY_TYPE_GRID is not supported");
#endif
    }
    
    default:
      throw_RTCError(RTC_ERROR_UNKNOWN,"invalid geometry type");
    }
    
    RTC_CATCH_END(device);
    return nullptr;
  }
  
  RTC_API void rtcSetGeometryUserPrimitiveCount(RTCGeometry hgeometry, unsigned int userPrimitiveCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryUserPrimitiveCount);
    RTC_VERIFY_HANDLE(hgeometry);
    
    if (unlikely(geometry->getType() != Geometry::GTY_USER_GEOMETRY))
      throw_RTCError(RTC_ERROR_INVALID_OPERATION,"operation only allowed for user geometries"); 

    geometry->setNumPrimitives(userPrimitiveCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTimeStepCount(RTCGeometry hgeometry, unsigned int timeStepCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTimeStepCount);
    RTC_VERIFY_HANDLE(hgeometry);

    if (timeStepCount > RTC_MAX_TIME_STEP_COUNT)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"number of time steps is out of range");
    
    geometry->setNumTimeSteps(timeStepCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryVertexAttributeCount(RTCGeometry hgeometry, unsigned int N)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryVertexAttributeCount);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setVertexAttributeCount(N);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTopologyCount(RTCGeometry hgeometry, unsigned int N)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTopologyCount);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setTopologyCount(N);
    RTC_CATCH_END2(geometry);
  }
 
  /*! sets the build quality of the geometry */
  RTC_API void rtcSetGeometryBuildQuality (RTCGeometry hgeometry, RTCBuildQuality quality) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryBuildQuality);
    RTC_VERIFY_HANDLE(hgeometry);
    if (quality != RTC_BUILD_QUALITY_LOW &&
        quality != RTC_BUILD_QUALITY_MEDIUM &&
        quality != RTC_BUILD_QUALITY_HIGH &&
        quality != RTC_BUILD_QUALITY_REFIT)
      throw std::runtime_error("invalid build quality");
    geometry->setBuildQuality(quality);
    RTC_CATCH_END2(geometry);
  }
  
  RTC_API void rtcSetGeometryMask (RTCGeometry hgeometry, unsigned int mask) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryMask);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setMask(mask);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometrySubdivisionMode (RTCGeometry hgeometry, unsigned topologyID, RTCSubdivisionMode mode) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometrySubdivisionMode);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setSubdivisionMode(topologyID,mode);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryVertexAttributeTopology(RTCGeometry hgeometry, unsigned int vertexAttributeID, unsigned int topologyID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryVertexAttributeTopology);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setVertexAttributeTopology(vertexAttributeID, topologyID);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryBuffer(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot, RTCFormat format, RTCBuffer hbuffer, size_t byteOffset, size_t byteStride, size_t itemCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    Ref<Buffer> buffer = (Buffer*)hbuffer;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_HANDLE(hbuffer);
    
    if (geometry->device != buffer->device)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"inputs are from different devices");
    
    if (itemCount > 0xFFFFFFFFu)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"buffer too large");
    
    geometry->setBuffer(type, slot, format, buffer, byteOffset, byteStride, (unsigned int)itemCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetSharedGeometryBuffer(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot, RTCFormat format, const void* ptr, size_t byteOffset, size_t byteStride, size_t itemCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetSharedGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    
    if (itemCount > 0xFFFFFFFFu)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"buffer too large");
    
    Ref<Buffer> buffer = new Buffer(geometry->device, itemCount*byteStride, (char*)ptr + byteOffset);
    geometry->setBuffer(type, slot, format, buffer, 0, byteStride, (unsigned int)itemCount);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void* rtcSetNewGeometryBuffer(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot, RTCFormat format, size_t byteStride, size_t itemCount)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetNewGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);

    if (itemCount > 0xFFFFFFFFu)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"buffer too large");
    
    /* vertex buffers need to get overallocated slightly as elements are accessed using SSE loads */
    size_t bytes = itemCount*byteStride;
    if (type == RTC_BUFFER_TYPE_VERTEX || type == RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE)
      bytes += (byteStride+15)%16 - byteStride;
      
    Ref<Buffer> buffer = new Buffer(geometry->device, bytes);
    geometry->setBuffer(type, slot, format, buffer, 0, byteStride, (unsigned int)itemCount);
    return buffer->data();
    RTC_CATCH_END2(geometry);
    return nullptr;
  }

  RTC_API void* rtcGetGeometryBufferData(RTCGeometry hgeometry, RTCBufferType type, unsigned int slot)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryBufferData);
    RTC_VERIFY_HANDLE(hgeometry);
    return geometry->getBuffer(type, slot);
    RTC_CATCH_END2(geometry);
    return nullptr;
  }
  
  RTC_API void rtcEnableGeometry (RTCGeometry hgeometry) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcEnableGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->enable();
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcUpdateGeometryBuffer (RTCGeometry hgeometry, RTCBufferType type, unsigned int slot) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcUpdateGeometryBuffer);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->updateBuffer(type, slot);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcDisableGeometry (RTCGeometry hgeometry) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcDisableGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->disable();
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryTessellationRate (RTCGeometry hgeometry, float tessellationRate)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryTessellationRate);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setTessellationRate(tessellationRate);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryUserData (RTCGeometry hgeometry, void* ptr) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryUserData);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setUserData(ptr);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void* rtcGetGeometryUserData (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry; // no ref counting here!
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryUserData);
    RTC_VERIFY_HANDLE(hgeometry);
    return geometry->getUserData();
    RTC_CATCH_END2(geometry);
    return nullptr;
  }

  RTC_API void rtcSetGeometryBoundsFunction (RTCGeometry hgeometry, RTCBoundsFunction bounds, void* userPtr)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryBoundsFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setBoundsFunction(bounds,userPtr);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryDisplacementFunction (RTCGeometry hgeometry, RTCDisplacementFunctionN displacement)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryDisplacementFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setDisplacementFunction(displacement);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryIntersectFunction (RTCGeometry hgeometry, RTCIntersectFunctionN intersect) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryIntersectFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setIntersectFunctionN(intersect);
    RTC_CATCH_END2(geometry);
  }

  RTC_API unsigned int rtcGetGeometryFirstHalfEdge(RTCGeometry hgeometry, unsigned int faceID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryFirstHalfEdge);
    return geometry->getFirstHalfEdge(faceID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryFace(RTCGeometry hgeometry, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryFace);
    return geometry->getFace(edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryNextHalfEdge(RTCGeometry hgeometry, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryNextHalfEdge);
    return geometry->getNextHalfEdge(edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryPreviousHalfEdge(RTCGeometry hgeometry, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryPreviousHalfEdge);
    return geometry->getPreviousHalfEdge(edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API unsigned int rtcGetGeometryOppositeHalfEdge(RTCGeometry hgeometry, unsigned int topologyID, unsigned int edgeID)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometryOppositeHalfEdge);
    return geometry->getOppositeHalfEdge(topologyID,edgeID);
    RTC_CATCH_END2(geometry);
    return -1;
  }

  RTC_API void rtcSetGeometryOccludedFunction (RTCGeometry hgeometry, RTCOccludedFunctionN occluded) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetOccludedFunctionN);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setOccludedFunctionN(occluded);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryIntersectFilterFunction (RTCGeometry hgeometry, RTCFilterFunctionN filter) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryIntersectFilterFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setIntersectionFilterFunctionN(filter);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcSetGeometryOccludedFilterFunction (RTCGeometry hgeometry, RTCFilterFunctionN filter) 
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcSetGeometryOccludedFilterFunction);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->setOcclusionFilterFunctionN(filter);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcInterpolate(const RTCInterpolateArguments* const args)
  {
    Geometry* geometry = (Geometry*) args->geometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcInterpolate);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(args->geometry);
#endif
    geometry->interpolate(args);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcInterpolateN(const RTCInterpolateNArguments* const args)
  {
    Geometry* geometry = (Geometry*) args->geometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcInterpolateN);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(args->geometry);
#endif
    geometry->interpolateN(args);
    RTC_CATCH_END2(geometry);
  }

  RTC_API void rtcCommitGeometry (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcCommitGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    return geometry->commit();
    RTC_CATCH_END2(geometry);
  }

  RTC_API unsigned int rtcAttachGeometry (RTCScene hscene, RTCGeometry hgeometry)
  {
    Scene* scene = (Scene*) hscene;
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcAttachGeometry);
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_HANDLE(hgeometry);
    if (scene->device != geometry->device)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"inputs are from different devices");
    return scene->bind(RTC_INVALID_GEOMETRY_ID,geometry);
    RTC_CATCH_END2(scene);
    return -1;
  }

  RTC_API void rtcAttachGeometryByID (RTCScene hscene, RTCGeometry hgeometry, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcAttachGeometryByID);
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_HANDLE(hgeometry);
    RTC_VERIFY_GEOMID(geomID);
    if (scene->device != geometry->device)
      throw_RTCError(RTC_ERROR_INVALID_ARGUMENT,"inputs are from different devices");
    scene->bind(geomID,geometry);
    RTC_CATCH_END2(scene);
  }
  
  RTC_API void rtcDetachGeometry (RTCScene hscene, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcDetachGeometry);
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_GEOMID(geomID);
    scene->detachGeometry(geomID);
    RTC_CATCH_END2(scene);
  }

  RTC_API void rtcRetainGeometry (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcRetainGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->refInc();
    RTC_CATCH_END2(geometry);
  }
  
  RTC_API void rtcReleaseGeometry (RTCGeometry hgeometry)
  {
    Geometry* geometry = (Geometry*) hgeometry;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcReleaseGeometry);
    RTC_VERIFY_HANDLE(hgeometry);
    geometry->refDec();
    RTC_CATCH_END2(geometry);
  }

  RTC_API RTCGeometry rtcGetGeometry (RTCScene hscene, unsigned int geomID)
  {
    Scene* scene = (Scene*) hscene;
    RTC_CATCH_BEGIN;
    RTC_TRACE(rtcGetGeometry);
#if defined(DEBUG)
    RTC_VERIFY_HANDLE(hscene);
    RTC_VERIFY_GEOMID(geomID);
#endif
    return (RTCGeometry) scene->get(geomID);
    RTC_CATCH_END2(scene);
    return nullptr;
  }
}
