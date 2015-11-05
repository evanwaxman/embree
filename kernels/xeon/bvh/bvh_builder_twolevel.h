// ======================================================================== //
// Copyright 2009-2015 Intel Corporation                                    //
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

#pragma once

#include "../bvh/bvh.h"
#include "../../common/scene_triangle_mesh.h"

namespace embree
{
  namespace isa
  {
    template<int N>
    class BVHNBuilderTwoLevel : public Builder
    {
      ALIGNED_CLASS;

      typedef BVHN<N> BVH;
      typedef typename BVH::Node Node;
      typedef typename BVH::NodeRef NodeRef;

    public:

      struct BuildRef
      {
      public:
        __forceinline BuildRef () {}

        __forceinline BuildRef (const BBox3fa& bounds, NodeRef node)
          : lower(bounds.lower), upper(bounds.upper), node(node)
        {
          if (node.isLeaf())
            lower.w = 0.0f;
          else
            lower.w = area(this->bounds());
        }

        __forceinline BBox3fa bounds () const {
          return BBox3fa(lower,upper);
        }

        friend bool operator< (const BuildRef& a, const BuildRef& b) {
          return a.lower.w < b.lower.w;
        }

      public:
        Vec3fa lower;
        Vec3fa upper;
        NodeRef node;
      };
      
      /*! Constructor. */
      BVHNBuilderTwoLevel (BVH* bvh, Scene* scene, const createTriangleMeshAccelTy createTriangleMeshAccel);
      
      /*! Destructor */
      ~BVHNBuilderTwoLevel ();
      
      /*! builder entry point */
      void build(size_t threadIndex, size_t threadCount);
      void deleteGeometry(size_t geomID);
      void clear();

      void open_sequential(size_t numPrimitives);
      
    public:
      BVH* bvh;
      std::vector<BVH*>& objects;
      std::vector<Builder*> builders;
      
    public:
      Scene* scene;
      createTriangleMeshAccelTy createTriangleMeshAccel;
      
      mvector<BuildRef> refs;
      mvector<PrimRef> prims;
      AlignedAtomicCounter32 nextRef;
    };
  }
}