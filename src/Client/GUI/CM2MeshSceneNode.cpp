// Copyright (C) 2002-2010 Nikolaus Gebhardt
// This file is part of the "Irrlicht Engine".
// For conditions of distribution and use, see copyright notice in irrlicht.h

#include "CM2MeshSceneNode.h"
#include "IVideoDriver.h"
#include "ISceneManager.h"
#include "S3DVertex.h"
#include "os.h"
#include "CShadowVolumeSceneNode.h"
#include "IAnimatedMeshMD3.h"
#include "CSkinnedMesh.h"
#include "IDummyTransformationSceneNode.h"
#include "IBoneSceneNode.h"
#include "IMaterialRenderer.h"
#include "IMesh.h"
#include "IMeshCache.h"
#include "IAnimatedMesh.h"
#include "quaternion.h"

#include "IGUIEnvironment.h"
#include "IGUIFont.h"
#include "ISceneCollisionManager.h"


using namespace irr;
//{
using namespace scene;
//{


//////////////////////////////////////////////////////////////////////////////////
// custom factory implementation
//////////////////////////////////////////////////////////////////////////////////

// id for our scene node, 'm2node'.
const int CM2MESHSCENENODE_ID = MAKE_IRR_ID('m','2','n','o','d','e');

// type name for our scene node
const char* CM2MeshSceneNodeTypeName = "CM2MeshSceneNode";

CM2MeshSceneNodeFactory::CM2MeshSceneNodeFactory(ISceneManager* mgr): Manager(mgr)
{
   // don't grab the scene manager here to prevent cyclic references
}

//! adds a scene node to the scene graph based on its type id
ISceneNode* CM2MeshSceneNodeFactory::addSceneNode( ESCENE_NODE_TYPE type, ISceneNode* parent)
{
   if (!parent)
      parent = Manager->getRootSceneNode();

   if (type == CM2MESHSCENENODE_ID)
   {
	   IAnimatedMesh* mesh=NULL;
      CM2MeshSceneNode* node = new CM2MeshSceneNode(mesh, parent, Manager, -1);
      node->drop();
      return node;
   }

   return 0;
}

//! adds a scene node to the scene graph based on its type name
ISceneNode* CM2MeshSceneNodeFactory::addSceneNode( const c8* typeName, ISceneNode* parent)
{
   return addSceneNode( getTypeFromName(typeName), parent );
}

ISceneNode* CM2MeshSceneNodeFactory::addM2SceneNode(IAnimatedMesh* mesh, ISceneNode* parent)
{
	if (!parent)
      parent = Manager->getRootSceneNode();

	CM2MeshSceneNode* node = new CM2MeshSceneNode(mesh, parent, Manager, -1);
	node->drop();
    return node;
}

//! returns amount of scene node types this factory is able to create
u32 CM2MeshSceneNodeFactory::getCreatableSceneNodeTypeCount() const
{
   return 1;
}

//! returns type of a createable scene node type
ESCENE_NODE_TYPE CM2MeshSceneNodeFactory::getCreateableSceneNodeType(u32 idx) const
{
   if (idx==0)
      return (ESCENE_NODE_TYPE)CM2MESHSCENENODE_ID;

   return ESNT_UNKNOWN;
}


//! returns type name of a createable scene node type 
const c8* CM2MeshSceneNodeFactory::getCreateableSceneNodeTypeName(u32 idx) const
{
   if (idx==0)
      return CM2MeshSceneNodeTypeName;

   return 0;
}

//! returns type name of a createable scene node type 
const c8* CM2MeshSceneNodeFactory::getCreateableSceneNodeTypeName(ESCENE_NODE_TYPE type) const
{
   if (type == CM2MESHSCENENODE_ID)
      return CM2MeshSceneNodeTypeName;

   return 0;
}

ESCENE_NODE_TYPE CM2MeshSceneNodeFactory::getTypeFromName(const c8* name) const
{
   if (!strcmp(name, CM2MeshSceneNodeTypeName))
      return (ESCENE_NODE_TYPE)CM2MESHSCENENODE_ID;

   return ESNT_UNKNOWN;
}

//////////////////////////////////////////////////
// CM2MeshSceneNode's functions
//////////////////////////////////////////////////
namespace irr
{
namespace scene
{

//! constructor
CM2MeshSceneNode::CM2MeshSceneNode(IAnimatedMesh* mesh,
		ISceneNode* parent, ISceneManager* mgr, s32 id,
		const core::vector3df& position,
		const core::vector3df& rotation,
		const core::vector3df& scale)
: IAnimatedMeshSceneNode(parent, mgr, id, position, rotation, scale), Mesh(0),
	StartFrame(0), EndFrame(0), FramesPerSecond(0.f),
	CurrentFrameNr(0.f), LastTimeMs(0),
	TransitionTime(0), Transiting(0.f), TransitingBlend(0.f),
	JointMode(EJUOR_NONE), JointsUsed(false),
	Looping(true), ReadOnlyMaterials(false), RenderFromIdentity(0),
	LoopCallBack(0), PassCount(0), Shadow(0),
	MD3Special ( 0 )
{
	#ifdef _DEBUG
	setDebugName("CM2MeshSceneNode");
	#endif

	FramesPerSecond = 25.f/1000.f;

	setMesh(mesh);
}


//! destructor
CM2MeshSceneNode::~CM2MeshSceneNode()
{
	if (MD3Special)
		MD3Special->drop ();

	if (Mesh)
		Mesh->drop();

	if (Shadow)
		Shadow->drop();

	//for (u32 i=0; i<JointChildSceneNodes.size(); ++i)
	//	if (JointChildSceneNodes[i])
	//		JointChildSceneNodes[i]->drop();

	if (LoopCallBack)
		LoopCallBack->drop();
}


//! Sets the current frame. From now on the animation is played from this frame.
void CM2MeshSceneNode::setCurrentFrame(f32 frame)
{
	// if you pass an out of range value, we just clamp it
	CurrentFrameNr = core::clamp ( frame, (f32)StartFrame, (f32)EndFrame );

	beginTransition(); //transit to this frame if enabled
}


//! Returns the currently displayed frame number.
f32 CM2MeshSceneNode::getFrameNr() const
{
	return CurrentFrameNr;
}


void CM2MeshSceneNode::buildFrameNr(u32 timeMs)
{
	if (Transiting!=0.f)
	{
		TransitingBlend += (f32)(timeMs) * Transiting;
		if (TransitingBlend > 1.f)
		{
			Transiting=0.f;
			TransitingBlend=0.f;
		}
	}

	if ((StartFrame==EndFrame))
	{
		CurrentFrameNr = (f32)StartFrame; //Support for non animated meshes
	}
	else if (Looping)
	{
		// play animation looped
		CurrentFrameNr += timeMs * FramesPerSecond;

		// We have no interpolation between EndFrame and StartFrame,
		// the last frame must be identical to first one with our current solution.
		if (FramesPerSecond > 0.f) //forwards...
		{
			if (CurrentFrameNr > EndFrame)
				CurrentFrameNr = StartFrame + fmod(CurrentFrameNr - StartFrame, (f32)(EndFrame-StartFrame));
		}
		else //backwards...
		{
			if (CurrentFrameNr < StartFrame)
				CurrentFrameNr = EndFrame - fmod(EndFrame - CurrentFrameNr, (f32)(EndFrame-StartFrame));
		}
	}
	else
	{
		// play animation non looped

		CurrentFrameNr += timeMs * FramesPerSecond;
		if (FramesPerSecond > 0.f) //forwards...
		{
			if (CurrentFrameNr > (f32)EndFrame)
			{
				CurrentFrameNr = (f32)EndFrame;
				if (LoopCallBack)
					LoopCallBack->OnAnimationEnd(this);
			}
		}
		else //backwards...
		{
			if (CurrentFrameNr < (f32)StartFrame)
			{
				CurrentFrameNr = (f32)StartFrame;
				if (LoopCallBack)
					LoopCallBack->OnAnimationEnd(this);
			}
		}
	}
}


void CM2MeshSceneNode::OnRegisterSceneNode()
{
	if (IsVisible)
	{
		// because this node supports rendering of mixed mode meshes consisting of
		// transparent and solid material at the same time, we need to go through all
		// materials, check of what type they are and register this node for the right
		// render pass according to that.

		video::IVideoDriver* driver = SceneManager->getVideoDriver();

		PassCount = 0;
		int transparentCount = 0;
		int solidCount = 0;

		// count transparent and solid materials in this scene node
		for (u32 i=0; i<Materials.size(); ++i)
		{
			video::IMaterialRenderer* rnd =
				driver->getMaterialRenderer(Materials[i].MaterialType);

			if (rnd && rnd->isTransparent())
				++transparentCount;
			else
				++solidCount;

			if (solidCount && transparentCount)
				break;
		}

		// register according to material types counted

		if (solidCount)
			SceneManager->registerNodeForRendering(this, scene::ESNRP_SOLID);

		if (transparentCount)
			SceneManager->registerNodeForRendering(this, scene::ESNRP_TRANSPARENT);

		ISceneNode::OnRegisterSceneNode();
	}
}

IMesh * CM2MeshSceneNode::getMeshForCurrentFrame()
{
	if(Mesh->getMeshType() != EAMT_SKINNED)
	{
		return Mesh->getMesh((s32)getFrameNr(), 255, StartFrame, EndFrame);
	}
	else
	{
		// As multiple scene nodes may be sharing the same skinned mesh, we have to
		// re-animate it every frame to ensure that this node gets the mesh that it needs.

		CSkinnedMesh* skinnedMesh = reinterpret_cast<CSkinnedMesh*>(Mesh);

		if (JointMode == EJUOR_CONTROL)//write to mesh
			skinnedMesh->transferJointsToMesh(JointChildSceneNodes);
		else
			skinnedMesh->animateMesh(getFrameNr(), 1.0f);

		// Update the skinned mesh for the current joint transforms.
		skinnedMesh->skinMesh();

		if (JointMode == EJUOR_READ)//read from mesh
		{
			skinnedMesh->recoverJointsFromMesh(JointChildSceneNodes);

			//---slow---
			for (u32 n=0;n<JointChildSceneNodes.size();++n)
				if (JointChildSceneNodes[n]->getParent()==this)
				{
					JointChildSceneNodes[n]->updateAbsolutePositionOfAllChildren(); //temp, should be an option
				}
		}

		if(JointMode == EJUOR_CONTROL)
		{
			// For meshes other than EJUOR_CONTROL, this is done by calling animateMesh()
			skinnedMesh->updateBoundingBox();
		}

		return skinnedMesh;
	}
}


//! OnAnimate() is called just before rendering the whole scene.
void CM2MeshSceneNode::OnAnimate(u32 timeMs)
{
	buildFrameNr(timeMs-LastTimeMs);

	if (Mesh)
	{
		scene::IMesh * mesh = getMeshForCurrentFrame();

		if (mesh)
			Box = mesh->getBoundingBox();
	}
	LastTimeMs = timeMs;

	IAnimatedMeshSceneNode::OnAnimate ( timeMs );
}


//! renders the node.
void CM2MeshSceneNode::render()
{
	video::IVideoDriver* driver = SceneManager->getVideoDriver();

	if (!Mesh || !driver)
		return;


	bool isTransparentPass =
		SceneManager->getSceneNodeRenderPass() == scene::ESNRP_TRANSPARENT;

	++PassCount;

	scene::IMesh* m = getMeshForCurrentFrame();

	if(m)
	{
		Box = m->getBoundingBox();
	}
	else
	{
		#ifdef _DEBUG
			os::Printer::log("Animated Mesh returned no mesh to render.", Mesh->getDebugName(), ELL_WARNING);
		#endif
	}

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);


	if (Shadow && PassCount==1)
		Shadow->updateShadowVolumes();

	// for debug purposes only:

	bool renderMeshes = true;
	video::SMaterial mat;
	if (DebugDataVisible && PassCount==1)
	{
		// overwrite half transparency
		if (DebugDataVisible & scene::EDS_HALF_TRANSPARENCY)
		{

			for (u32 i=0; i<m->getMeshBufferCount(); ++i)
			{
				scene::IMeshBuffer* mb = m->getMeshBuffer(i);
				mat = ReadOnlyMaterials ? mb->getMaterial() : Materials[i];
				mat.MaterialType = video::EMT_TRANSPARENT_ADD_COLOR;
				if (RenderFromIdentity)
					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix );
				else if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer*)mb)->Transformation);

				driver->setMaterial(mat);
				driver->drawMeshBuffer(mb);
			}
			renderMeshes = false;
		}
	}

	// render original meshes
	if (renderMeshes)
	{
		for (u32 i=0; i<m->getMeshBufferCount(); ++i)
		{
			video::IMaterialRenderer* rnd = driver->getMaterialRenderer(Materials[i].MaterialType);
			bool transparent = (rnd && rnd->isTransparent());

			// only render transparent buffer if this is the transparent render pass
			// and solid only in solid pass
            //PSEUWOW M2 RENDERING
            //Render only those submeshes that are actually active
            bool renderSubmesh = true;
            if(Mesh->getMeshType() == EAMT_M2)
              renderSubmesh = ((CM2Mesh*)Mesh)->getGeoSetRender(i);
			if (transparent == isTransparentPass && renderSubmesh) //PSEUWOW M2 RENDERING END
			{
				scene::IMeshBuffer* mb = m->getMeshBuffer(i);
				const video::SMaterial& material = ReadOnlyMaterials ? mb->getMaterial() : Materials[i];
				if (RenderFromIdentity)
					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix );
				else if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer*)mb)->Transformation);

				driver->setMaterial(material);
				driver->drawMeshBuffer(mb);
			}
		}
	}

	driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);

	// for debug purposes only:
	if (DebugDataVisible && PassCount==1)
	{
		video::SMaterial debug_mat;
		debug_mat.Lighting = false;
		debug_mat.AntiAliasing=0;
		driver->setMaterial(debug_mat);
		// show normals
		if (DebugDataVisible & scene::EDS_NORMALS)
		{
			core::vector3df normalizedNormal;
			const f32 DebugNormalLength = SceneManager->getParameters()->getAttributeAsFloat(DEBUG_NORMAL_LENGTH);
			const video::SColor DebugNormalColor = SceneManager->getParameters()->getAttributeAsColor(DEBUG_NORMAL_COLOR);

			// draw normals
			for (u32 g=0; g < m->getMeshBufferCount(); ++g)
			{
				const scene::IMeshBuffer* mb = m->getMeshBuffer(g);
				const u32 vSize = video::getVertexPitchFromType(mb->getVertexType());
				const video::S3DVertex* v = ( const video::S3DVertex*)mb->getVertices();
				const bool normalize = mb->getMaterial().NormalizeNormals;

				for (u32 i=0; i != mb->getVertexCount(); ++i)
				{
					normalizedNormal = v->Normal;
					if (normalize)
						normalizedNormal.normalize();

					driver->draw3DLine(v->Pos, v->Pos + (normalizedNormal * DebugNormalLength), DebugNormalColor);

					v = (const video::S3DVertex*) ( (u8*) v+vSize );
				}
			}
			driver->setTransform(video::ETS_WORLD, AbsoluteTransformation);
		}

		debug_mat.ZBuffer = video::ECFN_ALWAYS;
		debug_mat.Lighting = false;
		driver->setMaterial(debug_mat);

		if (DebugDataVisible & scene::EDS_BBOX)
			driver->draw3DBox(Box, video::SColor(255,255,255,255));

		// show bounding box
		if (DebugDataVisible & scene::EDS_BBOX_BUFFERS)
		{

			for (u32 g=0; g< m->getMeshBufferCount(); ++g)
			{
				const IMeshBuffer* mb = m->getMeshBuffer(g);

				if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer*)mb)->Transformation);
				driver->draw3DBox( mb->getBoundingBox(),
						video::SColor(255,190,128,128) );
			}
		}

		// show skeleton
		if (DebugDataVisible & scene::EDS_SKELETON)
		{
			if (Mesh->getMeshType() == EAMT_SKINNED|| Mesh->getMeshType() == EAMT_M2)
			{
				// draw skeleton
				//PSEUWOW
                ISceneCollisionManager* Coll = SceneManager->getSceneCollisionManager();
                irr::gui::IGUIFont* Font = SceneManager->getGUIEnvironment()->getBuiltInFont();
                //PSEUWOW END
				for (u32 g=0; g < ((ISkinnedMesh*)Mesh)->getAllJoints().size(); ++g)
				{
					ISkinnedMesh::SJoint *joint=((ISkinnedMesh*)Mesh)->getAllJoints()[g];
                    //PSEUWOW
                    core::vector3df a = joint->GlobalAnimatedMatrix.getTranslation()-core::vector3df(0.05,0.05,0.05);
                    core::vector3df b = joint->GlobalAnimatedMatrix.getTranslation()+core::vector3df(0.05,0.05,0.05);
                    core::aabbox3df marker = core::aabbox3df(a,b);
                    driver->draw3DBox(marker,video::SColor(255,51,66,255));

                    core::position2d<s32> pos = Coll->getScreenCoordinatesFrom3DPosition(a, SceneManager->getActiveCamera());
                    core::rect<s32> r(pos, core::dimension2d<s32>(1,1));
                    Font->draw(core::stringw(g).c_str(), r, video::SColor(255,51,255,66), true, true);
                    //PSEUWOW END

					for (u32 n=0;n<joint->Children.size();++n)
					{
						driver->draw3DLine(joint->GlobalAnimatedMatrix.getTranslation(),
								joint->Children[n]->GlobalAnimatedMatrix.getTranslation(),
								video::SColor(255,51,66,255));
					}
				}
			}

			// show tag for quake3 models
			if (Mesh->getMeshType() == EAMT_MD3)
			{
				IAnimatedMesh * arrow =
					SceneManager->addArrowMesh (
							"__tag_show",
							0xFF0000FF, 0xFF000088,
							4, 8, 5.f, 4.f, 0.5f,
							1.f);
				if (!arrow)
				{
					arrow = SceneManager->getMesh ( "__tag_show" );
				}
				IMesh *arrowMesh = arrow->getMesh(0);

				core::matrix4 matr;

				SMD3QuaternionTagList *taglist = ((IAnimatedMeshMD3*)Mesh)->getTagList(
						(s32)getFrameNr(), 255,
						getStartFrame(), getEndFrame());
				if (taglist)
				{
					for ( u32 ts = 0; ts != taglist->size(); ++ts )
					{
						(*taglist)[ts].setto(matr);

						driver->setTransform(video::ETS_WORLD, matr );

						for ( u32 a = 0; a != arrowMesh->getMeshBufferCount(); ++a )
							driver->drawMeshBuffer(arrowMesh->getMeshBuffer(a));
					}
				}
			}
		}

		// show mesh
		if (DebugDataVisible & scene::EDS_MESH_WIRE_OVERLAY)
		{
			debug_mat.Lighting = false;
			debug_mat.Wireframe = true;
			debug_mat.ZBuffer = video::ECFN_NEVER;
			driver->setMaterial(debug_mat);

			for (u32 g=0; g<m->getMeshBufferCount(); ++g)
			{
				const IMeshBuffer* mb = m->getMeshBuffer(g);
				if (RenderFromIdentity)
					driver->setTransform(video::ETS_WORLD, core::IdentityMatrix );
				else if (Mesh->getMeshType() == EAMT_SKINNED)
					driver->setTransform(video::ETS_WORLD, AbsoluteTransformation * ((SSkinMeshBuffer*)mb)->Transformation);
				driver->drawMeshBuffer(mb);
			}
		}
	}
}


//! Returns the current start frame number.
s32 CM2MeshSceneNode::getStartFrame() const
{
	return StartFrame;
}


//! Returns the current start frame number.
s32 CM2MeshSceneNode::getEndFrame() const
{
	return EndFrame;
}


//! sets the frames between the animation is looped.
//! the default is 0 - MaximalFrameCount of the mesh.
bool CM2MeshSceneNode::setFrameLoop(s32 begin, s32 end)
{
	const s32 maxFrameCount = Mesh->getFrameCount() - 1;
	if (end < begin)
	{
		StartFrame = core::s32_clamp(end, 0, maxFrameCount);
		EndFrame = core::s32_clamp(begin, StartFrame, maxFrameCount);
	}
	else
	{
		StartFrame = core::s32_clamp(begin, 0, maxFrameCount);
		EndFrame = core::s32_clamp(end, StartFrame, maxFrameCount);
	}
	if (FramesPerSecond < 0)
		setCurrentFrame((f32)EndFrame);
	else
		setCurrentFrame((f32)StartFrame);

	return true;
}


//! sets the speed with witch the animation is played
void CM2MeshSceneNode::setAnimationSpeed(f32 framesPerSecond)
{
	FramesPerSecond = framesPerSecond * 0.001f;
}


f32 CM2MeshSceneNode::getAnimationSpeed() const
{
	return FramesPerSecond * 1000.f;
}


//! returns the axis aligned bounding box of this node
const core::aabbox3d<f32>& CM2MeshSceneNode::getBoundingBox() const
{
	return Box;
}


//! returns the material based on the zero based index i. To get the amount
//! of materials used by this scene node, use getMaterialCount().
//! This function is needed for inserting the node into the scene hirachy on a
//! optimal position for minimizing renderstate changes, but can also be used
//! to directly modify the material of a scene node.
video::SMaterial& CM2MeshSceneNode::getMaterial(u32 i)
{
	if (i >= Materials.size())
		return ISceneNode::getMaterial(i);

	return Materials[i];
}



//! returns amount of materials used by this scene node.
u32 CM2MeshSceneNode::getMaterialCount() const
{
	return Materials.size();
}


//! Creates shadow volume scene node as child of this node
//! and returns a pointer to it.
IShadowVolumeSceneNode* CM2MeshSceneNode::addShadowVolumeSceneNode(const IMesh* shadowMesh,
						 s32 id, bool zfailmethod, f32 infinity)
{
	if (!SceneManager->getVideoDriver()->queryFeature(video::EVDF_STENCIL_BUFFER))
		return 0;

	if (Shadow)
		return Shadow;

	if (!shadowMesh)
		shadowMesh = Mesh; // if null is given, use the mesh of node

	Shadow = new CShadowVolumeSceneNode(shadowMesh, this, SceneManager, id,  zfailmethod, infinity);
	return Shadow;
}


//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
IBoneSceneNode* CM2MeshSceneNode::getJointNode(const c8* jointName)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
	{
		//ToDo: use different log for these
		//os::Printer::log("No mesh, or mesh not of skinned mesh type", ELL_WARNING);
		return 0;
	}

	checkJoints();

	ISkinnedMesh *skinnedMesh=(ISkinnedMesh*)Mesh;

	const s32 number = skinnedMesh->getJointNumber(jointName);

	if (number == -1)
	{
		//os::Printer::log("Joint with specified name not found in skinned mesh.", jointName, ELL_WARNING);
		return 0;
	}

	if ((s32)JointChildSceneNodes.size() <= number)
	{
		//os::Printer::log("Joint was found in mesh, but is not loaded into node", jointName, ELL_WARNING);
		return 0;
	}

	return getJointNode((u32)number);
}


//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a skinned mesh.
IBoneSceneNode* CM2MeshSceneNode::getJointNode(u32 jointID)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
	{
		// ToDo: use psuwow's loging for these too
		//os::Printer::log("No mesh, or mesh not of skinned mesh type", ELL_WARNING);
		return 0;
	}

	checkJoints();

	if (JointChildSceneNodes.size() <= jointID)
	{
		//os::Printer::log("Joint not loaded into node", ELL_WARNING);
		return 0;
	}

	return JointChildSceneNodes[jointID];
}

//! Gets joint count.
u32 CM2MeshSceneNode::getJointCount() const
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return 0;

	ISkinnedMesh *skinnedMesh=(ISkinnedMesh*)Mesh;

	return skinnedMesh->getJointCount();
}


//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a ms3d mesh.
ISceneNode* CM2MeshSceneNode::getMS3DJointNode(const c8* jointName)
{
	return  getJointNode(jointName);
}


//! Returns a pointer to a child node, which has the same transformation as
//! the corresponding joint, if the mesh in this scene node is a .x mesh.
ISceneNode* CM2MeshSceneNode::getXJointNode(const c8* jointName)
{
	return  getJointNode(jointName);
}


//! Removes a child from this scene node.
//! Implemented here, to be able to remove the shadow properly, if there is one,
//! or to remove attached childs.
bool CM2MeshSceneNode::removeChild(ISceneNode* child)
{
	if (child && Shadow == child)
	{
		Shadow->drop();
		Shadow = 0;
	}

	if (ISceneNode::removeChild(child))
	{
		if (JointsUsed) //stop weird bugs caused while changing parents as the joints are being created
		{
			for (u32 i=0; i<JointChildSceneNodes.size(); ++i)
			if (JointChildSceneNodes[i] == child)
			{
				JointChildSceneNodes[i] = 0; //remove link to child
				return true;
			}
		}
		return true;
	}

	return false;
}

//PSEUWOW
//! Starts a M2 animation.
bool CM2MeshSceneNode::setM2Animation(u32 animID)
{
    if (!Mesh || Mesh->getMeshType() != EAMT_M2)
        return false;

    CM2Mesh* m = (CM2Mesh*)Mesh;

    s32 begin = -1, end = -1;
    m->getFrameLoop(animID, begin, end); //Eventually different speeds are necessary?
    if(begin == -1 || end == -1)
      return false;

//     setAnimationSpeed( f32(speed) );
    setFrameLoop(begin, end);
    return true;
}
//PSEUWOW END

//! Starts a MD2 animation.
bool CM2MeshSceneNode::setMD2Animation(EMD2_ANIMATION_TYPE anim)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_MD2)
		return false;

	IAnimatedMeshMD2* md = (IAnimatedMeshMD2*)Mesh;

	s32 begin, end, speed;
	md->getFrameLoop(anim, begin, end, speed);

	setAnimationSpeed( f32(speed) );
	setFrameLoop(begin, end);
	return true;
}


//! Starts a special MD2 animation.
bool CM2MeshSceneNode::setMD2Animation(const c8* animationName)
{
	if (!Mesh || Mesh->getMeshType() != EAMT_MD2)
		return false;

	IAnimatedMeshMD2* md = (IAnimatedMeshMD2*)Mesh;

	s32 begin, end, speed;
	if (!md->getFrameLoop(animationName, begin, end, speed))
		return false;

	setAnimationSpeed( (f32)speed );
	setFrameLoop(begin, end);
	return true;
}


//! Sets looping mode which is on by default. If set to false,
//! animations will not be looped.
void CM2MeshSceneNode::setLoopMode(bool playAnimationLooped)
{
	Looping = playAnimationLooped;
}


//! Sets a callback interface which will be called if an animation
//! playback has ended. Set this to 0 to disable the callback again.
void CM2MeshSceneNode::setAnimationEndCallback(IAnimationEndCallBack* callback)
{
	if (callback == LoopCallBack)
		return;

	if (LoopCallBack)
		LoopCallBack->drop();

	LoopCallBack = callback;

	if (LoopCallBack)
		LoopCallBack->grab();
}


//! Sets if the scene node should not copy the materials of the mesh but use them in a read only style.
void CM2MeshSceneNode::setReadOnlyMaterials(bool readonly)
{
	ReadOnlyMaterials = readonly;
}


//! Returns if the scene node should not copy the materials of the mesh but use them in a read only style
bool CM2MeshSceneNode::isReadOnlyMaterials() const
{
	return ReadOnlyMaterials;
}


//! Writes attributes of the scene node.
void CM2MeshSceneNode::serializeAttributes(io::IAttributes* out, io::SAttributeReadWriteOptions* options) const
{
	IAnimatedMeshSceneNode::serializeAttributes(out, options);

	out->addString("Mesh", SceneManager->getMeshCache()->getMeshName(Mesh).getPath().c_str());
	out->addBool("Looping", Looping);
	out->addBool("ReadOnlyMaterials", ReadOnlyMaterials);
	out->addFloat("FramesPerSecond", FramesPerSecond);
	out->addInt("StartFrame", StartFrame);
	out->addInt("EndFrame", EndFrame);
}


//! Reads attributes of the scene node.
void CM2MeshSceneNode::deserializeAttributes(io::IAttributes* in, io::SAttributeReadWriteOptions* options)
{
	IAnimatedMeshSceneNode::deserializeAttributes(in, options);

	io::path oldMeshStr = SceneManager->getMeshCache()->getMeshName(Mesh);
	io::path newMeshStr = in->getAttributeAsString("Mesh");

	Looping = in->getAttributeAsBool("Looping");
	ReadOnlyMaterials = in->getAttributeAsBool("ReadOnlyMaterials");
	FramesPerSecond = in->getAttributeAsFloat("FramesPerSecond");
	StartFrame = in->getAttributeAsInt("StartFrame");
	EndFrame = in->getAttributeAsInt("EndFrame");

	if (newMeshStr != "" && oldMeshStr != newMeshStr)
	{
		IAnimatedMesh* newAnimatedMesh = SceneManager->getMesh(newMeshStr.c_str());

		if (newAnimatedMesh)
			setMesh(newAnimatedMesh);
	}

	// TODO: read animation names instead of frame begin and ends
}


//! Sets a new mesh
void CM2MeshSceneNode::setMesh(IAnimatedMesh* mesh)
{
	if (!mesh)
		return; // won't set null mesh

	if (Mesh != mesh)
	{
		if (Mesh)
			Mesh->drop();

		Mesh = mesh;

		// grab the mesh (it's non-null!)
		Mesh->grab();
	}

	// get materials and bounding box
	Box = Mesh->getBoundingBox();

	IMesh* m = Mesh->getMesh(0,0);
	if (m)
	{
		Materials.clear();
		Materials.reallocate(m->getMeshBufferCount());

		for (u32 i=0; i<m->getMeshBufferCount(); ++i)
		{
			IMeshBuffer* mb = m->getMeshBuffer(i);
			if (mb)
				Materials.push_back(mb->getMaterial());
			else
				Materials.push_back(video::SMaterial());
		}
	}

	// get start and begin time
	setFrameLoop ( 0, Mesh->getFrameCount() );
}


// returns the absolute transformation for a special MD3 Tag if the mesh is a md3 mesh,
// or the absolutetransformation if it's a normal scenenode
const SMD3QuaternionTag* CM2MeshSceneNode::getMD3TagTransformation( const core::stringc & tagname)
{
	return MD3Special ? MD3Special->AbsoluteTagList.get ( tagname ) : 0;
}


//! updates the absolute position based on the relative and the parents position
void CM2MeshSceneNode::updateAbsolutePosition()
{
	IAnimatedMeshSceneNode::updateAbsolutePosition();

	if (!Mesh || Mesh->getMeshType() != EAMT_MD3)
		return;

	SMD3QuaternionTagList *taglist;
	taglist = ( (IAnimatedMeshMD3*) Mesh )->getTagList ( (s32)getFrameNr(),255,getStartFrame (),getEndFrame () );
	if (taglist)
	{
		if (!MD3Special)
		{
			MD3Special = new SMD3Special();
		}

		SMD3QuaternionTag parent ( MD3Special->Tagname );
		if (Parent && Parent->getType() == ESNT_ANIMATED_MESH)
		{
			const SMD3QuaternionTag * p = ((IAnimatedMeshSceneNode*) Parent)->getMD3TagTransformation
									( MD3Special->Tagname );

			if (p)
				parent = *p;
		}

		SMD3QuaternionTag relative( RelativeTranslation, RelativeRotation );

		MD3Special->AbsoluteTagList.set_used ( taglist->size () );
		for ( u32 i=0; i!= taglist->size (); ++i )
		{
			MD3Special->AbsoluteTagList[i].position = parent.position + (*taglist)[i].position + relative.position;
			MD3Special->AbsoluteTagList[i].rotation = parent.rotation * (*taglist)[i].rotation * relative.rotation;
		}
	}
}


//! Set the joint update mode (0-unused, 1-get joints only, 2-set joints only, 3-move and set)
void CM2MeshSceneNode::setJointMode(E_JOINT_UPDATE_ON_RENDER mode)
{
	checkJoints();

	//if (mode<0) mode=0;
	//if (mode>3) mode=3;

	JointMode=mode;
}


/*
//! A test function for on the fly sorting of m2 scene meshes
void CM2MeshSceneNode::UpdateSubmeshOrder()
{
	// decals must have a distance prior to the farthest effect in their size bracket
	// if I store serveral extream points indices each point must be tested and the nearest one used. This would be most acurate
	// get distance from camera
	core::vector3df pos = Camera[2]->getPosition(); //The position of the camera. To compare with.
	scene::CM2Mesh* m = ((scene::CM2Mesh*)(((scene::IAnimatedMeshSceneNode*)(Model))->getMesh()));

	// loop through submeshes
	for (u32 i = 0; i <  m->getMeshBufferCount(); i++)
	{
		if (m->Skins[m->SkinID].Submeshes[i].Textures[0].BlendFlag < 3 && m->Skins[m->SkinID].Submeshes[i].Radius < 10) // only redistance small submeshes that aren't effects
		{
			scene::IMeshBuffer *submesh = m->getMeshBuffer(i);
			u32 j = m->Skins[m->SkinID].Submeshes[i].NearestVertex; // get index to vertice we are tracking
			core::vector3df v = submesh->getPosition(j) - pos;
			float dist = (v.X * v.X) + (v.Y * v.Y) + (v.Z * v.Z); //Squared distance
			
			// could also test against field of view ignoreing meshes outside it. usefull for sky maybe
			//float olddist = m->Skins[m->SkinID].Submeshes[i].Distance; // test if distance changed... It does yay.
			//log("%f", olddist);
			// Set the new distance of this submesh
			m->Skins[m->SkinID].Submeshes[i].Distance=dist;
		}
	}
	// Sort by distance 
	scene::CM2Mesh::submesh temp;
	for (int i = 0; i < m->Skins[m->SkinID].Submeshes.size()-1; i++) 
	{
		for (u16 j = i+1; j < m->Skins[m->SkinID].Submeshes.size(); j++)
		{
			if (m->Skins[m->SkinID].Submeshes[i].Distance < m->Skins[m->SkinID].Submeshes[j].Distance)
			{
				temp = m->Skins[m->SkinID].Submeshes[i];
				m->Skins[m->SkinID].Submeshes[i] = m->Skins[m->SkinID].Submeshes[j];
				m->Skins[m->SkinID].Submeshes[j] = temp;
			}
		}
	}
	//now we can manualy render each buffer in its new order
}
*/


//! Sets the transition time in seconds (note: This needs to enable joints, and setJointmode maybe set to 2)
//! you must call animateJoints(), or the mesh will not animate
void CM2MeshSceneNode::setTransitionTime(f32 time)
{
	if (time != 0.0f)
	{
		checkJoints();
		setJointMode(EJUOR_CONTROL);
		TransitionTime = (u32)core::floor32(time*1000.0f);
	}
}


//! render mesh ignoring its transformation. Used with ragdolls. (culling is unaffected)
void CM2MeshSceneNode::setRenderFromIdentity( bool On )
{
	RenderFromIdentity=On;
}


//! updates the joint positions of this mesh
void CM2MeshSceneNode::animateJoints(bool CalculateAbsolutePositions)
{
	checkJoints();

	if (Mesh && Mesh->getMeshType() == EAMT_SKINNED )
	{
		if (JointsUsed)
		{
			f32 frame = getFrameNr(); //old?

			CSkinnedMesh* skinnedMesh=reinterpret_cast<CSkinnedMesh*>(Mesh);

			skinnedMesh->transferOnlyJointsHintsToMesh( JointChildSceneNodes );

			skinnedMesh->animateMesh(frame, 1.0f);

			skinnedMesh->recoverJointsFromMesh( JointChildSceneNodes);

			//-----------------------------------------
			//		Transition
			//-----------------------------------------

			if (Transiting != 0.f)
			{
				//Check the array is big enough (not really needed)
				if (PretransitingSave.size()<JointChildSceneNodes.size())
				{
					for(u32 n=PretransitingSave.size(); n<JointChildSceneNodes.size(); ++n)
						PretransitingSave.push_back(core::matrix4());
				}

				for (u32 n=0; n<JointChildSceneNodes.size(); ++n)
				{
					//------Position------

					JointChildSceneNodes[n]->setPosition(
							core::lerp(
								PretransitingSave[n].getTranslation(),
								JointChildSceneNodes[n]->getPosition(),
								TransitingBlend));

					//------Rotation------

					//Code is slow, needs to be fixed up

					const core::quaternion RotationStart(PretransitingSave[n].getRotationDegrees()*core::DEGTORAD);
					const core::quaternion RotationEnd(JointChildSceneNodes[n]->getRotation()*core::DEGTORAD);

					core::quaternion QRotation;
					QRotation.slerp(RotationStart, RotationEnd, TransitingBlend);

					core::vector3df tmpVector;
					QRotation.toEuler(tmpVector);
					tmpVector*=core::RADTODEG; //convert from radians back to degrees
					JointChildSceneNodes[n]->setRotation( tmpVector );

					//------Scale------

					//JointChildSceneNodes[n]->setScale(
					//		core::lerp(
					//			PretransitingSave[n].getScale(),
					//			JointChildSceneNodes[n]->getScale(),
					//			TransitingBlend));
				}
			}

			if (CalculateAbsolutePositions)
			{
				//---slow---
				for (u32 n=0;n<JointChildSceneNodes.size();++n)
				{
					if (JointChildSceneNodes[n]->getParent()==this)
					{
						JointChildSceneNodes[n]->updateAbsolutePositionOfAllChildren(); //temp, should be an option
					}
				}
			}
		}
	}
}


/*!
*/
void CM2MeshSceneNode::checkJoints()
{
	if (!Mesh || Mesh->getMeshType() != EAMT_SKINNED)
		return;

	if (!JointsUsed)
	{
		//Create joints for SkinnedMesh

		((CSkinnedMesh*)Mesh)->createJoints(JointChildSceneNodes, this, SceneManager);
		((CSkinnedMesh*)Mesh)->recoverJointsFromMesh(JointChildSceneNodes);

		JointsUsed=true;
		JointMode=EJUOR_READ;
	}
}


/*!
*/
void CM2MeshSceneNode::beginTransition()
{
	if (!JointsUsed)
		return;

	if (TransitionTime != 0)
	{
		//Check the array is big enough
		if (PretransitingSave.size()<JointChildSceneNodes.size())
		{
			for(u32 n=PretransitingSave.size(); n<JointChildSceneNodes.size(); ++n)
				PretransitingSave.push_back(core::matrix4());
		}


		//Copy the position of joints
		for (u32 n=0;n<JointChildSceneNodes.size();++n)
			PretransitingSave[n]=JointChildSceneNodes[n]->getRelativeTransformation();

		Transiting = core::reciprocal((f32)TransitionTime);
	}
	TransitingBlend = 0.f;
}


/*!
*/
ISceneNode* CM2MeshSceneNode::clone(ISceneNode* newParent, ISceneManager* newManager)
{
	if (!newParent) newParent = Parent;
	if (!newManager) newManager = SceneManager;

	CM2MeshSceneNode * newNode =
		new CM2MeshSceneNode(Mesh, NULL, newManager, ID, RelativeTranslation,
						 RelativeRotation, RelativeScale);

	if ( newParent )
	{
		newNode->setParent(newParent); 	// not in constructor because virtual overload for updateAbsolutePosition won't be called
		newNode->drop();
	}

	newNode->cloneMembers(this, newManager);

	newNode->Materials = Materials;
	newNode->Box = Box;
	newNode->Mesh = Mesh;
	newNode->StartFrame = StartFrame;
	newNode->EndFrame = EndFrame;
	newNode->FramesPerSecond = FramesPerSecond;
	newNode->CurrentFrameNr = CurrentFrameNr;
	newNode->JointMode = JointMode;
	newNode->JointsUsed = JointsUsed;
	newNode->TransitionTime = TransitionTime;
	newNode->Transiting = Transiting;
	newNode->TransitingBlend = TransitingBlend;
	newNode->Looping = Looping;
	newNode->ReadOnlyMaterials = ReadOnlyMaterials;
	newNode->LoopCallBack = LoopCallBack;
	newNode->PassCount = PassCount;
	newNode->Shadow = Shadow;
	newNode->JointChildSceneNodes = JointChildSceneNodes;
	newNode->PretransitingSave = PretransitingSave;
	newNode->RenderFromIdentity = RenderFromIdentity;
	newNode->MD3Special = MD3Special;

	return newNode;
}


} // end namespace scene
} // end namespace irr

