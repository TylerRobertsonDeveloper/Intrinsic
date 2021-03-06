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

// Precompiled header file
#include "stdafx.h"

// PhysX includes
#include "extensions/PxDistanceJoint.h"
#include "PxRigidDynamic.h"
#include "extensions/PxRigidBodyExt.h"
#include "PxScene.h"
#include "extensions/PxJoint.h"

namespace Intrinsic
{
namespace Core
{
namespace GameStates
{
namespace
{
glm::vec2 _lastMousePos = glm::vec2();
bool _rightMouseButtonPressed = false;
bool _leftMouseButtonPressed = false;

// Gizmos
bool _translScaleXAxisSelected = false;
bool _translScaleYAxisSelected = false;
bool _translScaleZAxisSelected = false;
bool _anyTranslScaleAxisSelected = false;
glm::vec3 _initialPosOffset = glm::vec3(0.0f);
glm::vec3 _initialScale = glm::vec3(1.0f);
glm::quat _initialOrientation = glm::quat();
glm::vec3 _translScalePlaneNormal = glm::vec3(0.0f);

float _lastGizmoScale = 1.0f;

// Grid
glm::vec3 _gridPosition = glm::vec3(0.0f);
float _gridFade = 0.0f;

// Editing cams
CameraMode::Enum _cameraMode = CameraMode::kFreeFlight;
glm::vec3 _orbitCamCenter = glm::vec3(0.0f);
float _orbitRadius = 200.0f;
glm::vec3 _camVel = glm::vec3();
glm::vec3 _camAngVel = glm::vec3();

// Cloning
bool _cloningInProgress = false;

// Physics grabbing
physx::PxDistanceJoint* _pickingJoint = nullptr;
physx::PxRigidDynamic* _pickingDummyActor = nullptr;
float _pickingInitialDistance = 0.0f;

// <-

glm::vec3 _planeVectors[] = {
    glm::vec3(0.0f, 1.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f),
    glm::vec3(1.0f, 0.0f, 0.0f), glm::vec3(-1.0f, 0.0f, 0.0f),
    glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 0.0f, -1.0f)};

// <-

struct PerInstanceDataGizmoVertex
{
  glm::mat4 worldMatrix;
  glm::mat4 worldViewProjMatrix;
  glm::mat4 normalMatrix;

  glm::vec4 colorTintX;
  glm::vec4 colorTintY;
  glm::vec4 colorTintZ;
};

// <-

struct PerInstanceDataGizmoFragment
{
  float _dummy;
};

// <-

struct PerInstanceDataGridVertex
{
  glm::mat4 worldMatrix;
  glm::mat4 worldViewProjMatrix;
};

// <-

struct PerInstanceDataGridFragment
{
  glm::mat4 invWorldRotMatrix;
  glm::vec4 invWorldPos;
  glm::mat4 viewProjMatrix;
  glm::mat4 normalMatrix;

  glm::vec4 planeNormal;

  float gridSize;
  float fade;
};

// <-

Renderer::Vulkan::Resources::DrawCallRef _drawCallRefGizmo;
Renderer::Vulkan::Resources::DrawCallRef _drawCallRefGrid;

// <-

_INTR_INLINE bool isCurrentlySelectedEntityValid()
{
  return Editing::_currentlySelectedEntity.isValid() &&
         Components::NodeManager::getComponentForEntity(
             Editing::_currentlySelectedEntity)
             .isValid();
}

_INTR_INLINE void selectGizmoAxis(const glm::vec3& p_EntityWorldPos,
                                  const Math::Ray& p_WorldRay, bool& p_X,
                                  bool& p_Y, bool& p_Z)
{
  p_X = false;
  p_Y = false;
  p_Z = false;

  const float widthDepth = 0.05f * _lastGizmoScale;
  Math::AABB worldBoxX = {
      p_EntityWorldPos - glm::vec3(-0.5f, widthDepth, widthDepth),
      p_EntityWorldPos + glm::vec3(_lastGizmoScale, widthDepth, widthDepth)};
  Math::AABB worldBoxY = {
      p_EntityWorldPos - glm::vec3(widthDepth, -0.5f, widthDepth),
      p_EntityWorldPos + glm::vec3(widthDepth, _lastGizmoScale, widthDepth)};
  Math::AABB worldBoxZ = {
      p_EntityWorldPos - glm::vec3(widthDepth, widthDepth, -0.5f),
      p_EntityWorldPos + glm::vec3(widthDepth, widthDepth, _lastGizmoScale)};

  glm::vec3 xi0;
  glm::vec3 xi1;
  bool x = Math::calcIntersectRayAABB(p_WorldRay, worldBoxX, xi0, xi1);
  glm::vec3 yi0;
  glm::vec3 yi1;
  bool y = Math::calcIntersectRayAABB(p_WorldRay, worldBoxY, yi0, yi1);
  glm::vec3 zi0;
  glm::vec3 zi1;
  bool z = Math::calcIntersectRayAABB(p_WorldRay, worldBoxZ, zi0, zi1);

  float distX = FLT_MAX;
  if (x)
  {
    distX = glm::distance2(xi0, p_WorldRay.o);
  }

  float distY = FLT_MAX;
  if (y)
  {
    distY = glm::distance2(yi0, p_WorldRay.o);
  }

  float distZ = FLT_MAX;
  if (z)
  {
    distZ = glm::distance2(zi0, p_WorldRay.o);
  }

  if (distX < distY && distX < distZ)
  {
    p_X = true;
  }
  else if (distY < distX && distY < distZ)
  {
    p_Y = true;
  }
  else if (distZ < distX && distZ < distY)
  {
    p_Z = true;
  }
}

// <-

_INTR_INLINE glm::vec3 getAxisVector(bool p_X, bool p_Y, bool p_Z)
{
  if (p_X)
  {
    _INTR_ASSERT(!p_Y && !p_Z);
    return glm::vec3(1.0f, 0.0f, 0.0f);
  }
  else if (p_Y)
  {
    _INTR_ASSERT(!p_X && !p_Z);
    return glm::vec3(0.0f, 1.0f, 0.0f);
  }

  _INTR_ASSERT(!p_X && !p_Y);
  return glm::vec3(0.0f, 0.0f, 1.0f);
}

// <-

_INTR_INLINE glm::vec3
findBestTranslationPlaneNormal(const glm::vec3& p_Axis,
                               const glm::vec3& p_DirectionToCam)
{
  float maxDot = -FLT_MAX;
  uint32_t planeIdx = (uint32_t)-1;

  for (uint32_t i = 0u; i < 6u; ++i)
  {
    if (glm::abs(glm::dot(_planeVectors[i], p_Axis)) > 0.0f)
    {
      continue;
    }

    const float dot = glm::dot(_planeVectors[i], p_DirectionToCam);
    if (dot > maxDot)
    {
      maxDot = dot;
      planeIdx = i;
    }
  }

  return _planeVectors[planeIdx];
}

// <-

_INTR_INLINE glm::vec3 snapToGrid(const glm::vec3& p_Position, float p_GridSize)
{
  return glm::trunc(p_Position / p_GridSize) * p_GridSize;
}

// <-

_INTR_INLINE void updateCameraOrbit(float p_DeltaT)
{
  Components::CameraRef camRef = World::getActiveCamera();
  Components::NodeRef camNodeRef =
      Components::NodeManager::getComponentForEntity(
          Components::CameraManager::_entity(camRef));

  static const float mouseSens = glm::half_pi<float>() * 8.0f;
  static const float damping = 0.005f;

  glm::vec3& camRotEuler = Components::CameraManager::_descEulerAngles(camRef);
  glm::quat camRot = Components::NodeManager::_worldOrientation(camNodeRef) *
                     glm::quat(camRotEuler);
  glm::vec3 forward = camRot * glm::vec3(0.0f, 0.0f, 1.0f);

  // Init. orbit camera mode
  if (_cameraMode != CameraMode::kOrbit)
  {
    _camAngVel = glm::vec3(0.0f);
    _camVel = glm::vec3(0.0f);
    _orbitCamCenter = Components::NodeManager::_worldPosition(camNodeRef) +
                      _orbitRadius * forward;

    _cameraMode = CameraMode::kOrbit;
  }

  if (Input::System::getKeyStates()[Input::Key::kMouseRight] ==
      Input::KeyState::kPressed)
  {
    if (!_rightMouseButtonPressed)
    {
      _rightMouseButtonPressed = true;
    }
    else
    {
      const glm::vec2 mouseVelocity =
          mouseSens *
          (Input::System::getLastMousePosViewport() - _lastMousePos);
      _camAngVel += glm::vec3(mouseVelocity.x, mouseVelocity.y, 0.0f);
    }

    _lastMousePos = Input::System::getLastMousePosViewport();
  }
  else
  {
    _rightMouseButtonPressed = false;
  }

  if (Input::System::getKeyStates()[Input::Key::kAlt] ==
          Input::KeyState::kPressed &&
      Input::System::getKeyStates()[Input::Key::kCtrl] ==
          Input::KeyState::kPressed)
  {
    glm::vec3 up = camRot * glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::cross(forward, up);

    _orbitCamCenter += Editing::_cameraSpeed * _camAngVel.x * p_DeltaT * right;
    _orbitCamCenter += Editing::_cameraSpeed * -_camAngVel.y * p_DeltaT * up;
  }
  else if (Input::System::getKeyStates()[Input::Key::kAlt] ==
           Input::KeyState::kPressed)
  {
    camRotEuler.x += _camAngVel.y * p_DeltaT;
    camRotEuler.y += -_camAngVel.x * p_DeltaT;
  }
  else if (Input::System::getKeyStates()[Input::Key::kCtrl] ==
           Input::KeyState::kPressed)
  {
    _orbitRadius = glm::max(_orbitRadius -
                                Editing::_cameraSpeed *
                                    (_camAngVel.x - _camAngVel.y) * p_DeltaT,
                            0.1f);
  }

  Components::NodeManager::_position(camNodeRef) =
      _orbitCamCenter + camRot * glm::vec3(0.0f, 0.0f, -_orbitRadius);
  Components::NodeManager::updateTransforms(camNodeRef);

  Math::dampSimple(_camAngVel, damping, p_DeltaT);
}

// <-

_INTR_INLINE void updateCameraFreeFlight(float p_DeltaT)
{
  // Init. free flight mode
  if (_cameraMode != CameraMode::kFreeFlight)
  {
    _camAngVel = glm::vec3(0.0f);
    _camVel = glm::vec3(0.0f);

    _cameraMode = CameraMode::kFreeFlight;
  }

  Components::CameraRef camRef = World::getActiveCamera();
  Components::NodeRef camNodeRef =
      Components::NodeManager::getComponentForEntity(
          Components::CameraManager::_entity(camRef));

  static const float mouseSens = glm::half_pi<float>() * 8.0f;
  static const float damping = 0.005f;

  glm::vec3& camRotEuler = Components::CameraManager::_descEulerAngles(camRef);

  if (Input::System::getKeyStates()[Input::Key::kMouseRight] ==
      Input::KeyState::kPressed)
  {
    if (!_rightMouseButtonPressed)
    {
      _rightMouseButtonPressed = true;
    }
    else
    {
      const glm::vec2 mouseVelocity =
          mouseSens *
          (Input::System::getLastMousePosViewport() - _lastMousePos);
      _camAngVel += glm::vec3(mouseVelocity.y, -mouseVelocity.x, 0.0f);
    }

    _lastMousePos = Input::System::getLastMousePosViewport();
  }
  else
  {
    _rightMouseButtonPressed = false;
  }

  camRotEuler += _camAngVel * p_DeltaT;
  glm::quat camRot = Components::NodeManager::_worldOrientation(camNodeRef) *
                     glm::quat(camRotEuler);

  glm::vec3 forward = camRot * glm::vec3(0.0f, 0.0f, 1.0f);
  glm::vec3 up = camRot * glm::vec3(0.0f, 1.0f, 0.0f);
  glm::vec3 right = glm::cross(forward, up);

  float actualMoveSpeed = Editing::_cameraSpeed;
  if (Input::System::getKeyStates()[Input::Key::kShift] ==
      Input::KeyState::kPressed)
  {
    actualMoveSpeed *= 4.0f;
  }
  if (Input::System::getKeyStates()[Input::Key::kW] ==
      Input::KeyState::kPressed)
  {
    _camVel += forward * p_DeltaT * actualMoveSpeed;
  }
  if (Input::System::getKeyStates()[Input::Key::kA] ==
      Input::KeyState::kPressed)
  {
    _camVel += -right * p_DeltaT * actualMoveSpeed;
  }
  if (Input::System::getKeyStates()[Input::Key::kS] ==
      Input::KeyState::kPressed)
  {
    _camVel += -forward * p_DeltaT * actualMoveSpeed;
  }
  if (Input::System::getKeyStates()[Input::Key::kD] ==
      Input::KeyState::kPressed)
  {
    _camVel += right * p_DeltaT * actualMoveSpeed;
  }

  Components::NodeManager::_position(camNodeRef) += _camVel * p_DeltaT;
  Components::NodeManager::updateTransforms(camNodeRef);

  Math::dampSimple(_camVel, damping, p_DeltaT);
  Math::dampSimple(_camAngVel, damping, p_DeltaT);
}

// <-

_INTR_INLINE void handleGizmo(float p_DeltaT)
{
  if (Input::System::getKeyStates()[Input::Key::kMouseLeft] ==
      Input::KeyState::kPressed)
  {
    Components::CameraRef camRef = World::getActiveCamera();
    Components::NodeRef camNodeRef =
        Components::NodeManager::getComponentForEntity(
            Components::CameraManager::_entity(camRef));
    const glm::vec3& camPosition =
        Components::NodeManager::_worldPosition(camNodeRef);

    Components::NodeRef currentEntityNodeRef =
        Components::NodeManager::getComponentForEntity(
            Editing::_currentlySelectedEntity);
    const glm::vec3& entityWorldPos =
        Components::NodeManager::_worldPosition(currentEntityNodeRef);

    const Math::Ray worldRay = Math::calcMouseRay(
        camPosition, Input::System::getLastMousePosViewport(),
        Components::CameraManager::_inverseViewProjectionMatrix(camRef));

    // Initial click handling - select gizmo axis and calc. initial offsets
    if (!_leftMouseButtonPressed)
    {
      selectGizmoAxis(entityWorldPos, worldRay, _translScaleXAxisSelected,
                      _translScaleYAxisSelected, _translScaleZAxisSelected);
      _anyTranslScaleAxisSelected = _translScaleXAxisSelected ||
                                    _translScaleYAxisSelected ||
                                    _translScaleZAxisSelected;

      if (!_anyTranslScaleAxisSelected)
      {
        return;
      }

      const glm::vec3 axisVector =
          getAxisVector(_translScaleXAxisSelected, _translScaleYAxisSelected,
                        _translScaleZAxisSelected);
      const glm::vec3 dirToCam = glm::normalize(camPosition - entityWorldPos);
      _translScalePlaneNormal =
          findBestTranslationPlaneNormal(axisVector, dirToCam);

      glm::vec3 planeIntersectionPoint;
      bool intersects =
          Math::calcIntersectRayPlane(worldRay, _translScalePlaneNormal,
                                      entityWorldPos, planeIntersectionPoint);
      _INTR_ASSERT(intersects);

      _initialPosOffset = planeIntersectionPoint - entityWorldPos;
      _initialScale = Components::NodeManager::_size(currentEntityNodeRef);
      _initialOrientation =
          Components::NodeManager::_orientation(currentEntityNodeRef);

      _leftMouseButtonPressed = true;
    }

    // Calc. intersection between ray and selected axis plane
    glm::vec3 planeIntersectionPoint;
    if (Math::calcIntersectRayPlane(worldRay, _translScalePlaneNormal,
                                    entityWorldPos, planeIntersectionPoint))
    {
      Components::NodeRef parentNode =
          Components::NodeManager::_parent(currentEntityNodeRef);
      glm::vec3 newWorldPos =
          Components::NodeManager::_worldPosition(currentEntityNodeRef);
      glm::vec3 newSize = _initialScale;
      glm::vec3 newRotation = glm::vec3(0.0f);

      if (_translScaleXAxisSelected)
      {
        newSize.x +=
            planeIntersectionPoint.x - _initialPosOffset.x - newWorldPos.x;
        newRotation.x +=
            planeIntersectionPoint.x - _initialPosOffset.x - newWorldPos.x;
        newWorldPos.x = planeIntersectionPoint.x - _initialPosOffset.x;
      }
      else if (_translScaleYAxisSelected)
      {
        newSize.y +=
            planeIntersectionPoint.y - _initialPosOffset.y - newWorldPos.y;
        newRotation.y +=
            planeIntersectionPoint.y - _initialPosOffset.y - newWorldPos.y;
        newWorldPos.y = planeIntersectionPoint.y - _initialPosOffset.y;
      }
      else if (_translScaleZAxisSelected)
      {
        newSize.z +=
            planeIntersectionPoint.z - _initialPosOffset.z - newWorldPos.z;
        newRotation.z +=
            planeIntersectionPoint.z - _initialPosOffset.z - newWorldPos.z;
        newWorldPos.z = planeIntersectionPoint.z - _initialPosOffset.z;
      }

      // Snap to grid
      _gridPosition = snapToGrid(newWorldPos, Editing::_gridSize);
      newSize = snapToGrid(newSize, Editing::_gridSize);

      // Execute Gizmo transformation
      if (Editing::_editingMode == EditingMode::kTranslation)
      {
        if (parentNode.isValid())
        {
          Components::NodeManager::_position(currentEntityNodeRef) =
              glm::inverse(
                  Components::NodeManager::_worldOrientation(parentNode)) *
              (_gridPosition -
               Components::NodeManager::_worldPosition(parentNode));
        }
        else
        {
          Components::NodeManager::_position(currentEntityNodeRef) =
              _gridPosition;
        }
      }
      else if (Editing::_editingMode == EditingMode::kScale)
      {
        Components::NodeManager::_size(currentEntityNodeRef) = newSize;
      }
      else if (Editing::_editingMode == EditingMode::kRotation)
      {
        Components::NodeManager::_orientation(currentEntityNodeRef) =
            glm::quat(newRotation) * _initialOrientation;
      }

      Components::NodeManager::updateTransforms(currentEntityNodeRef);
    }
  }
  else
  {
    _translScaleXAxisSelected = false;
    _translScaleYAxisSelected = false;
    _translScaleZAxisSelected = false;
    _anyTranslScaleAxisSelected = false;
    _leftMouseButtonPressed = false;
  }
}
}

// Static members
Entity::EntityRef Editing::_currentlySelectedEntity;
EditingMode::Enum Editing::_editingMode = EditingMode::kDefault;
float Editing::_gridSize = 1.0f;
float Editing::_gizmoSize = 0.15f;
float Editing::_cameraSpeed = 150.0f;

// <-

void Editing::init()
{
  Resources::MeshRef meshRefGizmo =
      Resources::MeshManager::getResourceByName(_N(TranslateGizmo));
  Resources::MeshRef meshRefGrid =
      Resources::MeshManager::getResourceByName(_N(Plane));

  // Draw calls
  Renderer::Vulkan::Resources::DrawCallRefArray drawCallsToCreate;
  {
    _drawCallRefGizmo =
        Renderer::Vulkan::Resources::DrawCallManager::createDrawCallForMesh(
            _N(TranslateGizmo), meshRefGizmo,
            Renderer::Vulkan::Resources::MaterialManager::getResourceByName(
                _N(gizmo)),
            Renderer::Vulkan::MaterialPass::kDebugGizmo,
            sizeof(PerInstanceDataGizmoVertex),
            sizeof(PerInstanceDataGizmoFragment));
    drawCallsToCreate.push_back(_drawCallRefGizmo);

    _drawCallRefGrid =
        Renderer::Vulkan::Resources::DrawCallManager::createDrawCallForMesh(
            _N(Grid), meshRefGrid,
            Renderer::Vulkan::Resources::MaterialManager::getResourceByName(
                _N(grid)),
            Renderer::Vulkan::MaterialPass::kDebugGrid,
            sizeof(PerInstanceDataGridVertex),
            sizeof(PerInstanceDataGridFragment));
    drawCallsToCreate.push_back(_drawCallRefGrid);
  }

  Renderer::Vulkan::Resources::DrawCallManager::createResources(
      drawCallsToCreate);
}

// <-

void Editing::activate()
{
  Entity::EntityRef entityRef =
      Entity::EntityManager::getEntityByName(_N(MainCamera));
  _INTR_ASSERT(entityRef.isValid());
  Components::CameraRef cameraRef =
      Components::CameraManager::getComponentForEntity(entityRef);
  _INTR_ASSERT(cameraRef.isValid());

  World::setActiveCamera(cameraRef);
}

// <-

void Editing::deativate()
{
  if (_pickingJoint)
  {
    _pickingJoint->release();
    _pickingJoint = nullptr;
  }
  if (_pickingDummyActor)
  {
    _pickingDummyActor->release();
    _pickingDummyActor = nullptr;
  }
}

// <-

void Editing::updatePerInstanceData()
{
  Components::CameraRef camRef = World::getActiveCamera();
  _INTR_ASSERT(camRef.isValid());

  // Gizmo
  if (isCurrentlySelectedEntityValid())
  {
    Components::NodeRef nodeRef =
        Components::NodeManager::getComponentForEntity(
            _currentlySelectedEntity);
    glm::mat4 trans = glm::translate(
        glm::mat4(1.0f), Components::NodeManager::_worldPosition(nodeRef));

    _lastGizmoScale = Math::calcScreenSpaceScale(
        Components::NodeManager::_worldPosition(nodeRef),
        Components::CameraManager::_viewProjectionMatrix(camRef), _gizmoSize);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), glm::vec3(_lastGizmoScale));

    PerInstanceDataGizmoVertex perInstanceDataVertex;
    {
      perInstanceDataVertex.worldMatrix = trans * scale;
      perInstanceDataVertex.worldViewProjMatrix =
          Components::CameraManager::_viewProjectionMatrix(camRef) *
          perInstanceDataVertex.worldMatrix;
      perInstanceDataVertex.normalMatrix =
          Components::CameraManager::_viewMatrix(camRef);

      // Hightlight gizmo axis
      {
        const Components::NodeRef camNodeRef =
            Components::NodeManager::getComponentForEntity(
                Components::CameraManager::_entity(camRef));
        const Math::Ray worldRay = Math::calcMouseRay(
            Components::NodeManager::_worldPosition(camNodeRef),
            Input::System::getLastMousePosViewport(),
            Components::CameraManager::_inverseViewProjectionMatrix(camRef));

        bool x, y, z;
        selectGizmoAxis(Components::NodeManager::_worldPosition(nodeRef),
                        worldRay, x, y, z);

        perInstanceDataVertex.colorTintX =
            x ? glm::vec4(1.0f) : glm::vec4(0.5f);
        perInstanceDataVertex.colorTintY =
            y ? glm::vec4(1.0f) : glm::vec4(0.5f);
        perInstanceDataVertex.colorTintZ =
            z ? glm::vec4(1.0f) : glm::vec4(0.5f);
      }
    }

    PerInstanceDataGizmoFragment perInstanceDataFragment;
    {
      perInstanceDataFragment._dummy = 42.0f;
    }

    // Write to GPU memory
    Renderer::Vulkan::Resources::DrawCallRefArray dcsToUpdate = {
        _drawCallRefGizmo};
    Renderer::Vulkan::Resources::DrawCallManager::
        allocateAndUpdateUniformMemory(dcsToUpdate, &perInstanceDataVertex,
                                       sizeof(PerInstanceDataGizmoVertex),
                                       &perInstanceDataFragment,
                                       sizeof(PerInstanceDataGizmoFragment));
  }

  // Axis plane
  if (_gridFade > _INTR_EPSILON)
  {
    PerInstanceDataGridVertex perInstanceDataVertex;
    PerInstanceDataGridFragment perInstanceDataFragment;

    glm::mat4 rot = glm::mat4_cast(
        glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), _translScalePlaneNormal));
    glm::mat4 trans = glm::translate(glm::mat4(1.0f), _gridPosition);
    glm::mat4 scale =
        glm::scale(glm::mat4(1.0f), glm::vec3(_gridSize, 1.0f, _gridSize));

    {
      perInstanceDataVertex.worldMatrix = trans * rot * scale;
      perInstanceDataVertex.worldViewProjMatrix =
          Components::CameraManager::_viewProjectionMatrix(camRef) *
          perInstanceDataVertex.worldMatrix;
    }

    {
      perInstanceDataFragment.invWorldRotMatrix = glm::inverse(rot);
      perInstanceDataFragment.invWorldPos = glm::vec4(-_gridPosition, 1.0f);
      perInstanceDataFragment.planeNormal =
          glm::vec4(_translScalePlaneNormal, 0.0f);
      perInstanceDataFragment.viewProjMatrix =
          Components::CameraManager::_viewProjectionMatrix(camRef);
      perInstanceDataFragment.normalMatrix =
          Components::CameraManager::_viewMatrix(camRef);
      perInstanceDataFragment.gridSize = _gridSize;
      perInstanceDataFragment.fade = _gridFade;
    }

    // Write to GPU memory
    Renderer::Vulkan::Resources::DrawCallRefArray dcsToUpdate = {
        _drawCallRefGrid};
    Renderer::Vulkan::Resources::DrawCallManager::
        allocateAndUpdateUniformMemory(dcsToUpdate, &perInstanceDataVertex,
                                       sizeof(PerInstanceDataGridVertex),
                                       &perInstanceDataFragment,
                                       sizeof(PerInstanceDataGridFragment));
  }
}

// <-

void Editing::findVisibleEditingDrawCalls(Dod::RefArray& p_DrawCalls)
{
  if (isCurrentlySelectedEntityValid())
  {
    if (_editingMode == EditingMode::kTranslation ||
        _editingMode == EditingMode::kScale ||
        _editingMode == EditingMode::kRotation)
    {
      p_DrawCalls.push_back(_drawCallRefGizmo);
    }
  }

  if (_gridFade > _INTR_EPSILON)
  {
    p_DrawCalls.push_back(_drawCallRefGrid);
  }
}

// <-

void Editing::update(float p_DeltaT)
{
  _INTR_PROFILE_CPU("Game States", "Editing Game State Update");

  // Fade grid in/out
  static const float fadeDurationInSeconds = 1.0f;
  if (_anyTranslScaleAxisSelected)
  {
    if (_gridFade < 1.0f)
    {
      _gridFade += p_DeltaT * fadeDurationInSeconds;
      _gridFade = glm::min(_gridFade, 1.0f);
    }
  }
  else
  {
    if (_gridFade > 0.0f)
    {
      _gridFade -= p_DeltaT * fadeDurationInSeconds;
      _gridFade = glm::max(_gridFade, 0.0f);
    }
  }

  if (isCurrentlySelectedEntityValid())
  {
    if (_editingMode == EditingMode::kTranslation)
    {
      handleGizmo(p_DeltaT);

      // Clone node
      if (Input::System::getKeyStates()[Input::Key::kAlt] ==
              Input::KeyState::kPressed &&
          Input::System::getKeyStates()[Input::Key::kMouseLeft] ==
              Input::KeyState::kPressed &&
          !_cloningInProgress)
      {
        _currentlySelectedEntity =
            World::cloneNodeFull(Components::NodeManager::getComponentForEntity(
                _currentlySelectedEntity));
        Resources::EventManager::queueEventIfNotExisting(
            _N(CurrentlySelectedEntityChanged));

        _cloningInProgress = true;
      }
      else if (Input::System::getKeyStates()[Input::Key::kMouseLeft] ==
               Input::KeyState::kReleased)
      {
        _cloningInProgress = false;
      }
    }
    else if (_editingMode == EditingMode::kRotation)
    {
      handleGizmo(p_DeltaT);
    }
    else if (_editingMode == EditingMode::kScale)
    {
      handleGizmo(p_DeltaT);
    }
  }

  if (_editingMode == EditingMode::kDefault)
  {
    // Physics picking
    if (Input::System::getKeyStates()[Input::Key::kMouseLeft] ==
        Input::KeyState::kPressed)
    {
      Components::CameraRef camRef = World::getActiveCamera();
      Components::NodeRef camNodeRef =
          Components::NodeManager::getComponentForEntity(
              Components::CameraManager::_entity(camRef));
      const glm::vec3& camPosition =
          Components::NodeManager::_worldPosition(camNodeRef);

      const Math::Ray worldRay = Math::calcMouseRay(
          camPosition, Input::System::getLastMousePosViewport(),
          Components::CameraManager::_inverseViewProjectionMatrix(camRef));

      if (_pickingJoint == nullptr && _pickingDummyActor == nullptr)
      {
        // Init physics dummy picking actor
        _pickingDummyActor = Physics::System::_pxPhysics->createRigidDynamic(
            physx::PxTransform(physx::PxIdentity));
        _INTR_ASSERT(_pickingDummyActor);

        _pickingDummyActor->setRigidDynamicFlag(
            physx::PxRigidDynamicFlag::eKINEMATIC, true);

        physx::PxSphereGeometry sphereGeometry;
        sphereGeometry.radius = 0.1f;

        physx::PxShape* shape = _pickingDummyActor->createShape(
            sphereGeometry, *Components::RigidBodyManager::_defaultMaterial);
        _INTR_ASSERT(shape);

        physx::PxRigidBodyExt::updateMassAndInertia(*_pickingDummyActor, 1.0f);
        Physics::System::_pxScene->addActor(*_pickingDummyActor);

        physx::PxRaycastHit hit;
        if (PhysxHelper::raycast(worldRay, hit, 1000.0f,
                                 physx::PxQueryFlag::eDYNAMIC))
        {
          _pickingInitialDistance = hit.distance;
          _pickingJoint = physx::PxDistanceJointCreate(
              *Physics::System::_pxPhysics, hit.actor,
              physx::PxTransform(physx::PxIdentity), _pickingDummyActor,
              physx::PxTransform(physx::PxIdentity));
        }
      }

      _pickingDummyActor->setKinematicTarget(
          physx::PxTransform(PhysxHelper::convert(
              worldRay.o + worldRay.d * _pickingInitialDistance)));
    }
    else
    {
      // Release joint and helper actor
      if (_pickingJoint)
      {
        _pickingJoint->release();
        _pickingJoint = nullptr;
      }
      if (_pickingDummyActor)
      {
        _pickingDummyActor->release();
        _pickingDummyActor = nullptr;
      }
    }
  }
  else if (_editingMode == EditingMode::kSelection)
  {
    // Pick nodes
    if (Input::System::getKeyStates()[Input::Key::kMouseLeft] ==
        Input::KeyState::kPressed)
    {
      Components::NodeRef pickedNode =
          Renderer::Vulkan::RenderPass::PerPixelPicking::pickNode(
              Input::System::getLastMousePosViewport());

      if (pickedNode.isValid())
      {
        Entity::EntityRef entityToSelect =
            Components::NodeManager::_entity(pickedNode);

        if (entityToSelect != _currentlySelectedEntity)
        {
          _currentlySelectedEntity = entityToSelect;
          Resources::EventManager::queueEventIfNotExisting(
              _N(CurrentlySelectedEntityChanged));
        }
      }
    }
  }

  // Focus on selected object
  if (Input::System::getKeyStates()[Input::Key::kF] ==
          Input::KeyState::kPressed &&
      _currentlySelectedEntity.isValid())
  {
    Components::NodeRef camNodeRef =
        Components::NodeManager::getComponentForEntity(
            Components::CameraManager::_entity(World::getActiveCamera()));
    Components::NodeRef entityNodeRef =
        Components::NodeManager::getComponentForEntity(
            _currentlySelectedEntity);

    const glm::vec3 halfExtents = Math::calcAABBHalfExtent(
        Components::NodeManager::_worldAABB(entityNodeRef));
    const float camOffset = glm::length(halfExtents) * 2.0f;

    const glm::vec3 newPosition =
        Math::calcAABBCenter(
            Components::NodeManager::_worldAABB(entityNodeRef)) -
        camOffset *
            Components::CameraManager::_forward(World::getActiveCamera());

    const float blendFactor = p_DeltaT * 2.0f;
    Components::NodeManager::_position(camNodeRef) =
        (1.0f - blendFactor) * Components::NodeManager::_position(camNodeRef) +
        blendFactor * newPosition;
  }

  if (Input::System::getKeyStates()[Input::Key::kAlt] ==
          Input::KeyState::kPressed ||
      Input::System::getKeyStates()[Input::Key::kCtrl] ==
          Input::KeyState::kPressed)
  {
    updateCameraOrbit(p_DeltaT);
  }
  else
  {
    updateCameraFreeFlight(p_DeltaT);
  }
}
}
}
}
