// Intrinsic
// Copyright (c) 2016 Benjamin Glatzel
//
// This program is free software : you can redistribute it and / or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.

#pragma once

namespace Intrinsic
{
namespace Core
{
namespace Components
{
typedef Dod::Ref PostEffectVolumeRef;
typedef _INTR_ARRAY(PostEffectVolumeRef) PostEffectVolumeRefArray;

struct PostEffectVolumeData : Dod::Components::ComponentDataBase
{
  PostEffectVolumeData()
      : Dod::Components::ComponentDataBase(
            _INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT)
  {
    descPostEffectName.resize(_INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT);
    descRadius.resize(_INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT);
    descBlendRange.resize(_INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT);
  }

  _INTR_ARRAY(Name) descPostEffectName;
  _INTR_ARRAY(float) descRadius;
  _INTR_ARRAY(float) descBlendRange;
};

struct PostEffectVolumeManager
    : Dod::Components::ComponentManagerBase<
          PostEffectVolumeData,
          _INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT>
{
  static void init();

  // <-

  _INTR_INLINE static PostEffectVolumeRef
  createPostEffectVolume(Entity::EntityRef p_ParentEntity)
  {
    PostEffectVolumeRef ref = Dod::Components::ComponentManagerBase<
        PostEffectVolumeData,
        _INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT>::
        _createComponent(p_ParentEntity);
    return ref;
  }

  // <-

  _INTR_INLINE static void resetToDefault(MeshRef p_Ref)
  {
    _descPostEffectName(p_Ref) = "";
    _descRadius(p_Ref) = 10.0f;
    _descBlendRange(p_Ref) = 2.0f;
  }

  // <-

  _INTR_INLINE static void
  destroyPostEffectVolume(PostEffectVolumeRef p_PostEffectVolume)
  {
    Dod::Components::ComponentManagerBase<
        PostEffectVolumeData,
        _INTR_MAX_POST_EFFECT_CONTROLLER_COMPONENT_COUNT>::
        _destroyComponent(p_PostEffectVolume);
  }

  // <-

  _INTR_INLINE static void compileDescriptor(PostEffectVolumeRef p_Ref,
                                             rapidjson::Value& p_Properties,
                                             rapidjson::Document& p_Document)
  {
    p_Properties.AddMember(
        "postEffectName",
        _INTR_CREATE_PROP(p_Document, _N(PostEffectVolume), _N(string),
                          _descPostEffectName(p_Ref), false, false),
        p_Document.GetAllocator());
    p_Properties.AddMember(
        "radius", _INTR_CREATE_PROP(p_Document, _N(PostEffectVolume), _N(float),
                                    _descRadius(p_Ref), false, false),
        p_Document.GetAllocator());
    p_Properties.AddMember("blendRange",
                           _INTR_CREATE_PROP(p_Document, _N(PostEffectVolume),
                                             _N(float), _descBlendRange(p_Ref),
                                             false, false),
                           p_Document.GetAllocator());
  }

  // <-

  _INTR_INLINE static void initFromDescriptor(PostEffectVolumeRef p_Ref,
                                              rapidjson::Value& p_Properties)
  {
    if (p_Properties.HasMember("postEffectName"))
      _descPostEffectName(p_Ref) =
          JsonHelper::readPropertyName(p_Properties["postEffectName"]);
    if (p_Properties.HasMember("radius"))
      _descRadius(p_Ref) =
          JsonHelper::readPropertyFloat(p_Properties["radius"]);
    if (p_Properties.HasMember("blendRange"))
      _descBlendRange(p_Ref) =
          JsonHelper::readPropertyFloat(p_Properties["blendRange"]);
  }

  // <-

  static void
  blendPostEffects(const PostEffectVolumeRefArray& p_PostEffectVolumes);

  // <-

  // Members refs.
  // Intrinsic

  // Description

  _INTR_INLINE static Name& _descPostEffectName(PostEffectVolumeRef p_Ref)
  {
    return _data.descPostEffectName[p_Ref._id];
  }
  _INTR_INLINE static float& _descRadius(PostEffectVolumeRef p_Ref)
  {
    return _data.descRadius[p_Ref._id];
  }
  _INTR_INLINE static float& _descBlendRange(PostEffectVolumeRef p_Ref)
  {
    return _data.descBlendRange[p_Ref._id];
  }
};
}
}
}
