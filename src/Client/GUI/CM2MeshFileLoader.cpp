//#define _DEBUG 1
#include <iostream>
#include "MemoryDataHolder.h"
#include "MemoryInterface.h"
#include "CM2MeshFileLoader.h"
#include "common.h"
#include "windows.h"

namespace irr
{
namespace scene
{

CM2MeshFileLoader::CM2MeshFileLoader(IrrlichtDevice* device):Device(device)
{
    Mesh = NULL;

}

CM2MeshFileLoader::~CM2MeshFileLoader()
{

}

core::vector3df fixCoordSystem(core::vector3df v)
{
        return core::vector3df(v.X, v.Z, v.Y);
}

core::quaternion fixQuaternion(core::quaternion q)
{
        return core::quaternion(q.X, q.Z, q.Y, q.W);
}
bool CM2MeshFileLoader::isALoadableFileExtension(const io::path& filename)const
{
    return core::hasFileExtension ( filename, "m2" );
}


//! creates/loads an animated mesh from the file.
//! \return Pointer to the created mesh. Returns 0 if loading failed.
//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
//! See IUnknown::drop() for more information.
IAnimatedMesh* CM2MeshFileLoader::createMesh(io::IReadFile* file)
{
    if(!file)
        return 0;
    MeshFile = file;
    AnimatedMesh = new scene::CM2Mesh();

    if ( load() )
    {
        AnimatedMesh->finalize();
    }
    else
    {
        AnimatedMesh->drop();
        AnimatedMesh = 0;
    }

    return AnimatedMesh;
}

void CM2MeshFileLoader::ReadVertices()
{
    //Vertices.  Global data
    if(!M2MVertices.empty())
        M2MVertices.clear();

    ModelVertex tempM2MVert;
    MeshFile->seek(header.Vertices.ofs);

    for(u32 i =0;i<header.Vertices.num;i++)
    {
        MeshFile->read(&tempM2MVert,sizeof(ModelVertex));
        tempM2MVert.pos = fixCoordSystem(tempM2MVert.pos);
        tempM2MVert.normal = fixCoordSystem(tempM2MVert.normal);
        M2MVertices.push_back(tempM2MVert);
    }
    DEBUG(logdebug("Read %u/%u Vertices",M2MVertices.size(),header.Vertices.num));
}

void CM2MeshFileLoader::ReadViewData(io::IReadFile* file)
{
    SkinData TempSkin;
	   

	//Vertex indices of a specific view.  Local to currentView
	u16 tempM2Index;
		
	file->seek(currentView.Index.ofs);
	for(u32 i =0;i<currentView.Index.num;i++)
	{
		file->read(&tempM2Index,sizeof(u16));
		TempSkin.M2MIndices.push_back(tempM2Index);
	}
	DEBUG(logdebug("Read %u/%u Indices",TempSkin.M2MIndices.size(),currentView.Index.num));


	//Triangles. Data Points point to the Vertex Indices, not the vertices themself. 3 Points = 1 Triangle, Local to currentView
	u16 tempM2Triangle;
		
	file->seek(currentView.Triangle.ofs);
	for(u32 i =0;i<currentView.Triangle.num;i++)
	{
		file->read(&tempM2Triangle,sizeof(u16));
		TempSkin.M2MTriangles.push_back(tempM2Triangle);
	}
	DEBUG(logdebug("Read %u/%u Triangles",TempSkin.M2MTriangles.size(),currentView.Triangle.num));
	//Submeshes, Local to currentView
	ModelViewSubmesh tempM2Submesh;
		
	file->seek(currentView.Submesh.ofs);
	for(u32 i =0;i<currentView.Submesh.num;i++)
	{
		file->read(&tempM2Submesh,sizeof(ModelViewSubmesh)-(header.version==0x100?16:0));
		TempSkin.M2MSubmeshes.push_back(tempM2Submesh);
		DEBUG(logdebug("Submesh %u MeshPartID %u",i,tempM2Submesh.meshpartId));
	//    std::cout<< "Submesh " <<i<<" ID "<<tempM2Submesh.meshpartId<<" starts at V/T "<<tempM2Submesh.ofsVertex<<"/"<<tempM2Submesh.ofsTris<<" and has "<<tempM2Submesh.nVertex<<"/"<<tempM2Submesh.nTris<<" V/T\n";
	}
	DEBUG(logdebug("Read %u/%u Submeshes",TempSkin.M2MSubmeshes.size(),currentView.Submesh.num));


	//Texture units. Local to currentView
	TextureUnit tempM2TexUnit;
		
	file->seek(currentView.Tex.ofs);
	for(u32 i=0;i<currentView.Tex.num;i++)
	{
		file->read(&tempM2TexUnit,sizeof(TextureUnit));
		TempSkin.M2MTextureUnit.push_back(tempM2TexUnit);
	DEBUG(logdebug(" TexUnit %u: Submesh: %u %u Render Flag: %u TextureUnitNumber: %u %u TTU: %u",i,tempM2TexUnit.submeshIndex1,tempM2TexUnit.submeshIndex2, tempM2TexUnit.renderFlagsIndex, tempM2TexUnit.TextureUnitNumber, tempM2TexUnit.TextureUnitNumber2 ,tempM2TexUnit.textureIndex));
	}
	DEBUG(logdebug("Read %u Texture Unit entries for View %u",TempSkin.M2MTextureUnit.size(), M2MSkins.size()-1));

	M2MSkins.push_back(TempSkin);

}

void CM2MeshFileLoader::ReadABlock(AnimBlock &ABlock, u8 datatype, u8 datanum)
{
// datatypes: 0 = float, 1 = short
// datatype & datanum: datatype 0, datanum 3 --> read 3 floats
    MeshFile->read(&ABlock.header,4);
    if(header.version < 0x108)//Read/Skip Interpolation Ranges for preWOTLK
      MeshFile->read(&ABlock.header.InterpolationRanges,sizeof(numofs));
    MeshFile->read(&ABlock.header.TimeStamp,sizeof(numofs));
    MeshFile->read(&ABlock.header.Values,sizeof(numofs));
    u32 offset_next_entry = MeshFile->getPos();
//     logdebug("%u %u %u %u",datatype, datanum, ABlock.header.TimeStamp.num,ABlock.header.Values.num);

    core::array<numofs> data_offsets;
    numofs tempNumOfs;
    io::IReadFile* AnimFile;

    //Read Timestamps
    if(header.version < 0x108)
      data_offsets.push_back(ABlock.header.TimeStamp);
    else
    {
      MeshFile->seek(ABlock.header.TimeStamp.ofs);
      for(u32 i =0;i < ABlock.header.TimeStamp.num; i++)
      {
        MeshFile->read(&tempNumOfs,sizeof(numofs));
        data_offsets.push_back(tempNumOfs);
      }
    }
    for(u32 i = 0; i < data_offsets.size(); i++)
    {
      tempNumOfs = data_offsets[i];

      if((M2MAnimations[i].flags & 0x20) == 0 && (M2MAnimations[i].flags & 0x40))
        continue;

      u32 offset = 0;
      if(header.version >= 0x108)
        offset = M2MAnimations[i].start;//HACK: Squashing WoTLK multitimelines into one single timeline

      if(header.version >= 0x108 && M2MAnimfiles[i] != 0)
        AnimFile = M2MAnimfiles[i];
      else
        AnimFile = MeshFile;
      AnimFile->seek(tempNumOfs.ofs);
      u32 tempTS;
      for(u32 j = 0; j < tempNumOfs.num; j++)
      {
        AnimFile->read(&tempTS,sizeof(u32));
        ABlock.timestamps.push_back(tempTS+offset);
      }
    }

    data_offsets.clear();
    //Read Values
    if(header.version < 0x108)
      data_offsets.push_back(ABlock.header.Values);
    else
    {
      MeshFile->seek(ABlock.header.Values.ofs);
      for(u32 i =0;i < ABlock.header.Values.num; i++)
      {
        MeshFile->read(&tempNumOfs,sizeof(numofs));
        data_offsets.push_back(tempNumOfs);
      }
    }
    for(u32 i = 0; i < data_offsets.size(); i++)
    {
      tempNumOfs = data_offsets[i];
      if((M2MAnimations[i].flags & 0x20) == 0 && (M2MAnimations[i].flags & 0x40))
        continue;
      if(header.version >= 0x108 && M2MAnimfiles[i] != 0)
        AnimFile = M2MAnimfiles[i];
      else
        AnimFile = MeshFile;
      AnimFile->seek(tempNumOfs.ofs);
      s16 tempShort;
      s32 tempInt;
      f32 tempFloat;
      for(u32 j = 0; j < tempNumOfs.num * datanum; j++)
      {
        switch(datatype)
        {
          case ABDT_FLOAT:
            AnimFile->read(&tempFloat,sizeof(f32));
            break;
          case ABDT_SHORT:
            MeshFile->read(&tempShort, sizeof(s16));
            tempFloat=(tempShort>0?tempShort-32767:tempShort+32767)/32767.0f;
            break;
          case ABDT_INT:
            AnimFile->read(&tempInt,sizeof(s32));
            tempFloat=tempInt * 1.0f;
            break;
        }
        ABlock.values.push_back(tempFloat);
      }
    }

    MeshFile->seek(offset_next_entry);
}


void CM2MeshFileLoader::ReadBones()
{
    //Bones. This is global data
    if(!M2MBones.empty())
    {
        M2MBones.clear();
    }
    MeshFile->seek(header.Bones.ofs);
    for(u32 i=0;i<header.Bones.num;i++)
    {
        Bone tempBone;
        MeshFile->read(&tempBone,12+(header.version==0x100?0:4));

        ReadABlock(tempBone.translation,ABDT_FLOAT,3);
        ReadABlock(tempBone.rotation,(header.version>=0x104?ABDT_SHORT:ABDT_FLOAT),4);
        ReadABlock(tempBone.scaling,ABDT_FLOAT,3);

        MeshFile->read(&tempBone.PivotPoint,sizeof(core::vector3df));
        tempBone.PivotPoint=fixCoordSystem(tempBone.PivotPoint);
        M2MBones.push_back(tempBone);
        DEBUG(logdebug("Bone %u Parent %u PP %f %f %f Flags %X",i,tempBone.parentBone,tempBone.PivotPoint.X,tempBone.PivotPoint.Y,tempBone.PivotPoint.Z, tempBone.flags));
    }
    DEBUG(logdebug("Read %u Bones",M2MBones.size()));

}


void CM2MeshFileLoader::ReadColors()
{
    if (!M2MVertexColor.empty())
    {
        M2MVertexColor.clear();
    }
    //read vertex color and alpha offsets
    logdebug("Colors");
    MeshFile->seek(header.Colors.ofs);
    for(u32 i=0;i<header.Colors.num;i++)
    {
      VertexColor tempVC;
      logdebug("Color %u",i);
      ReadABlock(tempVC.Colors,ABDT_FLOAT,3);
      ReadABlock(tempVC.Alpha,ABDT_SHORT,1);
      M2MVertexColor.push_back(tempVC);
    }
}


void CM2MeshFileLoader::ReadLights()
{
    if (!M2MLights.empty())
    {
        M2MLights.clear();
    }
    //read Lights
    MeshFile->seek(header.Lights.ofs);

    for(u32 i=0;i<header.Lights.num;i++)
    {
        M2Light tempLight;
        MeshFile->read(&tempLight,16);//2xshort + vector3df
        ReadABlock(tempLight.AmbientColor,ABDT_FLOAT,3);
        ReadABlock(tempLight.AmbientIntensity,ABDT_FLOAT,1);
        ReadABlock(tempLight.DiffuseColor,ABDT_FLOAT,3);
        ReadABlock(tempLight.DiffuseIntensity,ABDT_FLOAT,1);
        ReadABlock(tempLight.AttenuationStart,ABDT_FLOAT,1);
        ReadABlock(tempLight.AttenuationEnd,ABDT_FLOAT,1);
        ReadABlock(tempLight.Unknown,ABDT_INT,1);
        M2MLights.push_back(tempLight);
    }
}


void CM2MeshFileLoader::ReadCameras()
{
    if (!M2MCameras.empty())
    {
        M2MCameras.clear();
    }
    //read Cameras
    MeshFile->seek(header.Cameras.ofs);

    for(u32 i=0;i<header.Cameras.num;i++)
    {
        M2Camera tempCamera;
        MeshFile->read(&tempCamera,16);
        ReadABlock(tempCamera.CamPosTranslation,ABDT_FLOAT,9); // 3 vector3df per CamPosTranslation element so 9 floats
	    MeshFile->seek(header.Cameras.ofs+36);
        MeshFile->read(&tempCamera.Position,12);
	    ReadABlock(tempCamera.CamTargetTranslation,ABDT_FLOAT,9); // 3 vector3df per CamTargetTranslation element so 9 floats
	    MeshFile->seek(header.Cameras.ofs+68);
        MeshFile->read(&tempCamera.Target,12);
        ReadABlock(tempCamera.Scale,ABDT_FLOAT,3);
    }
}


void CM2MeshFileLoader::ReadUVAnimations()
{
    if (!M2MUVAnimations.empty())
    {
        M2MUVAnimations.clear();
    }
    //read Teture UVAnimations
    MeshFile->seek(header.TexAnims.ofs);
    for(u32 i=0;i<header.TexAnims.num;i++)
    {
      UVAnimation tempUV;
      ReadABlock(tempUV.Translation,ABDT_FLOAT,3);
      ReadABlock(tempUV.Rotation,ABDT_SHORT,4);
      ReadABlock(tempUV.Scaling,ABDT_FLOAT,3);
      M2MUVAnimations.push_back(tempUV);
    }
}


void CM2MeshFileLoader::ReadAnimationData()
{
//     //Global Sequences. This is global data
//     u32 tempGlobalSeq;
//     if(!M2MGlobalSequences.empty())
//     {
//         M2MGlobalSequences.clear();
//     }
//     MeshFile->seek(header.GlobalSequences.ofs);
//     for(u32 i=0;i<header.GlobalSequences.num;i++)
//     {
//         MeshFile->read(&tempGlobalSeq,sizeof(u32));
//         M2MGlobalSequences.push_back(tempGlobalSeq);
//         DEBUG(logdebug("Global Sequence %u End %u",i,tempGlobalSeq));
//     }
//     DEBUG(logdebug("Read %u Global Sequence entries",M2MGlobalSequences.size()));

     //BoneLookupTable. This is global data.  Used by submeshes to indicate the bones they are associated with
     u16 tempBoneLookup;
     if(!M2MBoneLookupTable.empty())
     {
         M2MBoneLookupTable.clear();
     }
     MeshFile->seek(header.BoneLookupTable.ofs);
     for(u32 i=0;i<header.BoneLookupTable.num;i++)
     {
         MeshFile->read(&tempBoneLookup,sizeof(u16));
         M2MBoneLookupTable.push_back(tempBoneLookup);
         DEBUG(logdebug("BoneLookupTable %u Value %u",i,tempBoneLookup));
     }
     DEBUG(logdebug("Read %u BoneLookupTable entries",M2MBoneLookupTable.size()));

     //SkeleBoneLookupTable. This is global data
     u16 tempSkeleBoneLookup;
     if(!M2MSkeleBoneLookupTable.empty())
     {
         M2MSkeleBoneLookupTable.clear();
     }
     MeshFile->seek(header.SkelBoneLookup.ofs);
     for(u32 i=0;i<header.SkelBoneLookup.num;i++)
     {
         MeshFile->read(&tempSkeleBoneLookup,sizeof(u16));
         M2MSkeleBoneLookupTable.push_back(tempSkeleBoneLookup);
         DEBUG(logdebug("SkeleBoneLookupTable %u Value %u",i,tempSkeleBoneLookup));
     }
     DEBUG(logdebug("Read %u SkeleBoneLookupTable entries",M2MSkeleBoneLookupTable.size()));

	 /* Index into SkelBoneLookupTable. |  Name of that index.   refrenced by M2MBones element(-1 if none) to indicate name of bone
						00                 "ArmL" 
						01                 "ArmR" 
						02                 "ShoulderL" 
						03                 "ShoulderR" 
						04                 "SpineLow" 
						05                 "Waist" 
						06                 "Head" 
						07                 "Jaw" 
						08                 "IndexFingerR" 
						09                 "MiddleFingerR" 
						10                 "PinkyFingerR" 
						11                 "RingFingerR" 
						12                 "ThumbR" 
						13                 "IndexFingerL" 
						14                 "MiddleFingerL" 
						15                 "PinkyFingerL" 
						16                 "RingFingerL" 
						17                 "ThumbL" 
						18                 "$BTH" 
						19                 "$CSR" 
						20                 "$CSL" 
						21                 "_Breath" 
						22                 "_Name" 
						23                 "_NameMount" 
						24                 "$CHD" 
						25                 "$CCH" 
						26                 "Root" 
						27                 "Wheel1" 
						28                 "Wheel2" 
						29                 "Wheel3" 
						30                 "Wheel4" 
						31                 "Wheel5" 
						32                 "Wheel6" 
						33                 "Wheel7" 
						34                 "Wheel8" 
						*/

    //Animations. This is global data
    u32 laststart = 0;
    if(!M2MAnimations.empty())
    {
        M2MAnimations.clear();
    }
    MeshFile->seek(header.Animations.ofs);
    for(u32 i=0;i<header.Animations.num;i++)
    {
        Animation tempAnimation;
        if(header.version < 0x108)
        {
          RawAnimation tempRaw;
          MeshFile->read(&tempRaw,sizeof(RawAnimation));

          tempAnimation.animationID = tempRaw.animationID;
          tempAnimation.probability = tempRaw.probability / 32767.0f;
          tempAnimation.start = tempRaw.start;
          tempAnimation.end = tempRaw.end;
        }
        else
        {
          RawAnimationWOTLK tempRaw;
          MeshFile->read(&tempRaw,sizeof(RawAnimationWOTLK));
          tempAnimation.animationID = tempRaw.animationID;
          tempAnimation.subanimationID = tempRaw.subanimationID;
          tempAnimation.flags = tempRaw.flags;
          tempAnimation.probability = tempRaw.probability / 32767.0f;
          tempAnimation.start = laststart;
          tempAnimation.end = laststart + tempRaw.length;
          laststart += tempRaw.length + 1000;
          if((tempAnimation.flags & 0x20) == 0)
          {
            std::string AnimName = MeshFile->getFileName().c_str();
            c8 ext[13];
            sprintf(ext,"%04d-%02d.anim",tempAnimation.animationID,tempAnimation.subanimationID);
            AnimName = AnimName.substr(0, AnimName.length()-3) + ext;
            io::IReadFile* AnimFile = io::IrrCreateIReadFileBasic(Device, AnimName.c_str());
            if (!AnimFile)
            {
                logerror("Error! Anim file not found: %s", AnimName.c_str());
                if(tempAnimation.flags & 0x40)
                {
                  logerror("We actually expected this error - Where is the data for this animation???");
                }
                AnimFile=0;
            }
            M2MAnimfiles.push_back(AnimFile);
          }
          else
            M2MAnimfiles.push_back(0);
        }
        M2MAnimations.push_back(tempAnimation);
        DEBUG(logdebug("Animation %u Id %u Start %u End %u",i,tempAnimation.animationID,tempAnimation.start,tempAnimation.end));
    }
    DEBUG(logdebug("Read %u Animations",M2MAnimations.size()));
//     //Animation Lookup. This is global data
//     s16 tempAniLookup;
//     if(!M2MAnimationLookup.empty())
//     {
//         M2MAnimationLookup.clear();
//     }
//     MeshFile->seek(header.AnimationLookup.ofs);
//     for(u32 i=0;i<header.AnimationLookup.num;i++)
//     {
//         MeshFile->read(&tempAniLookup,sizeof(s16));
//         M2MAnimationLookup.push_back(tempAniLookup);
//         DEBUG(logdebug("Animation Lookup %u Id %u",i,tempAniLookup));
//     }
//     DEBUG(logdebug("Read %u AnimationLookup",M2MAnimationLookup.size()));
}

void CM2MeshFileLoader::ReadTextureDefinitions()
{
    //Texture Lookup table. This is global data
    u16 tempM2TexLookup;
    if(!M2MTextureLookup.empty())
    {
        M2MTextureLookup.clear();
    }
    MeshFile->seek(header.TexLookup.ofs);
    for(u32 i=0;i<header.TexLookup.num;i++)
    {
        MeshFile->read(&tempM2TexLookup,sizeof(u16));
        M2MTextureLookup.push_back(tempM2TexLookup);
        DEBUG(logdebug("Texture %u Type %u",i,tempM2TexLookup));
    }
    DEBUG(logdebug("Read %u Texture lookup entries",M2MTextureLookup.size()));

    //Texture Definitions table. This is global data
    TextureDefinition tempM2TexDef;
    if(!M2MTextureDef.empty())
    {
        M2MTextureDef.clear();
    }
    MeshFile->seek(header.Textures.ofs);
    for(u32 i=0;i<header.Textures.num;i++)
    {
        MeshFile->read(&tempM2TexDef,sizeof(TextureDefinition));
        M2MTextureDef.push_back(tempM2TexDef);
        DEBUG(logdebug("Texture %u Type %u",i,tempM2TexDef.texType));
    }
    DEBUG(logdebug("Read %u Texture Definition entries",M2MTextureDef.size()));

    //Render Flags table. This is global data
    RenderFlags tempM2RF;
    if(!M2MRenderFlags.empty())
    {
        M2MRenderFlags.clear();
    }
    MeshFile->seek(header.TexFlags.ofs);
    for(u32 i=0;i<header.TexFlags.num;i++)
    {
        MeshFile->read(&tempM2RF,sizeof(RenderFlags));
        M2MRenderFlags.push_back(tempM2RF);
        DEBUG(logdebug("Flag %u: (%u, %u)",i,tempM2RF.blending,tempM2RF.flags));
    }
    DEBUG(logdebug("Read %u Renderflags",M2MRenderFlags.size()));

    if(!M2MTextureFiles.empty())
        M2MTextureFiles.clear();

    std::string tempTexFileName="";
    M2MTextureFiles.reallocate(M2MTextureDef.size());
    for(u32 i=0; i<M2MTextureDef.size(); i++)
    {
        tempTexFileName.resize(M2MTextureDef[i].texFileLen + 1);
        MeshFile->seek(M2MTextureDef[i].texFileOfs);
        MeshFile->read((void*)tempTexFileName.data(),M2MTextureDef[i].texFileLen);
        M2MTextureFiles.push_back("");
        M2MTextureFiles[i]=tempTexFileName.c_str();
        DEBUG(logdebug("Texture: %u %u (%s/%s) @ %u(%u)",i,M2MTextureFiles.size(),M2MTextureFiles[i].c_str(),tempTexFileName.c_str(),M2MTextureDef[i].texFileOfs,M2MTextureDef[i].texFileLen));
    }
}


void CM2MeshFileLoader::CopyAnimationsToMesh(CM2Mesh * CurrentMesh)
{
	for(u32 i=0;i<M2MAnimations.size();i++)
	{
	  CurrentMesh->newAnimation(M2MAnimations[i].animationID,M2MAnimations[i].start,M2MAnimations[i].end,M2MAnimations[i].probability);
	}


	scene::CM2Mesh::SJoint* Joint;
	for(u32 i=0;i<M2MBones.size();i++)  
	{
	  if(M2MBones[i].parentBone == -1)
	  {
		  ParentJoint=(scene::CM2Mesh::SJoint*)0;
	  }
	  else
	  {
		  ParentJoint=CurrentMesh->getAllJoints()[M2MBones[i].parentBone];
	  }
	  Joint=CurrentMesh->addJoint(ParentJoint);
	  //Set Joint Name
	  switch (M2MBones[i].SkelBoneIndex) // -1 = no name
	  {
		case 0: 
		Joint->Name = "ArmL";
		break;
		case 1: 
		Joint->Name = "ArmR";
		break;
		case 2: 
		Joint->Name = "ShoulderL";
		break; 
		case 3: 
		Joint->Name = "ShoulderR";
		break;
		case 4: 
		Joint->Name = "SpineLow"; 
		break;
		case 5: 
		Joint->Name = "Waist"; 
		break;
		case 6: 
		Joint->Name = "Head"; 
		break;
		case 7: 
		Joint->Name = "Jaw"; 
		break;
		case 8: 
		Joint->Name = "IndexFingerR";
		break;
		case 9: 
		Joint->Name = "MiddleFingerR"; 
		break;
		case 10: 
		Joint->Name = "PinkyFingerR"; 
		break;
		case 11: 
		Joint->Name = "RingFingerR"; 
		break;
		case 12: 
		Joint->Name = "ThumbR"; 
		break;
		case 13: 
		Joint->Name = "IndexFingerL";
		break; 
		case 14: 
		Joint->Name = "MiddleFingerL"; 
		break;
		case 15: 
		Joint->Name = "PinkyFingerL"; 
		break;
		case 16: 
		Joint->Name = "RingFingerL"; 
		break;
		case 17: 
		Joint->Name = "ThumbL"; 
		break;
		case 18: 
		Joint->Name = "$BTH"; 
		break;
		case 19: 
		Joint->Name = "$CSR"; 
		break;
		case 20: 
		Joint->Name = "$CSL"; 
		break;
		case 21: 
		Joint->Name = "_Breath"; 
		break;
		case 22: 
		Joint->Name = "_Name"; 
		break;
		case 23: 
		Joint->Name = "_NameMount"; 
		break;
		case 24: 
		Joint->Name = "$CHD";
		break;
		case 25: 
		Joint->Name = "$CCH"; 
		break;
		case 26: 
		Joint->Name = "Root"; 
		break;
		case 27: 
		Joint->Name = "Wheel1";
		break;
		case 28: 
		Joint->Name = "Wheel2";
		break;
		case 29: 
		Joint->Name = "Wheel3"; 
		break;
		case 30: 
		Joint->Name = "Wheel4"; 
		break;
		case 31: 
		Joint->Name = "Wheel5"; 
		break;
		case 32: 
		Joint->Name = "Wheel6"; 
		break;
		case 33: 
		Joint->Name = "Wheel7"; 
		break;
		case 34: 
		Joint->Name = "Wheel8"; 
		break;
	  }

	  if(M2MBones[i].translation.timestamps.size()>0)
	  {
		for(u32 j=0;j<M2MBones[i].translation.timestamps.size();j++)
		{
		  scene::CM2Mesh::SPositionKey* pos=CurrentMesh->addPositionKey(Joint);
		  pos->frame=M2MBones[i].translation.timestamps[j];
		  pos->position=fixCoordSystem(core::vector3df(M2MBones[i].translation.values[j*3],M2MBones[i].translation.values[j*3+1],M2MBones[i].translation.values[j*3+2]));
		}
	  }
	  if(M2MBones[i].rotation.timestamps.size()>0)
	  {
		for(u32 j=0;j<M2MBones[i].rotation.timestamps.size();j++)
		{
		  scene::CM2Mesh::SRotationKey* rot=CurrentMesh->addRotationKey(Joint);
		  rot->frame=M2MBones[i].rotation.timestamps[j];
		  core::quaternion tempQ=core::quaternion(M2MBones[i].rotation.values[j*4+0],M2MBones[i].rotation.values[j*4+1],M2MBones[i].rotation.values[j*4+2],M2MBones[i].rotation.values[j*4+3]);
		  tempQ = fixQuaternion(tempQ);
		  tempQ.normalize();
		  rot->rotation=tempQ;
		}
	  }

	  if(M2MBones[i].scaling.timestamps.size()>0)
	  {
		for(u32 j=0;j<M2MBones[i].scaling.timestamps.size();j++)
		{
		  scene::CM2Mesh::SScaleKey* scale=CurrentMesh->addScaleKey(Joint);
		  scale->frame=M2MBones[i].scaling.timestamps[j];
		  scale->scale=core::vector3df(M2MBones[i].scaling.values[j*3],M2MBones[i].scaling.values[j*3+1],M2MBones[i].scaling.values[j*3+2]);
		}
	  }

	  Joint->Animatedposition=M2MBones[i].PivotPoint;
	  Joint->Animatedscale=core::vector3df(1.0f,1.0f,1.0f);
	  Joint->Animatedrotation=core::quaternion(0.0f,0.0f,0.0f,1.0f);

	  core::matrix4 positionMatrix;
	  positionMatrix.setTranslation( Joint->Animatedposition );

	  core::matrix4 rotationMatrix = Joint->Animatedrotation.getMatrix();

	  core::matrix4 scaleMatrix;
	  scaleMatrix.setScale( Joint->Animatedscale );

	  Joint->GlobalMatrix = positionMatrix * rotationMatrix * scaleMatrix;
	}
}


void CM2MeshFileLoader::BuildANewSubMesh(CM2Mesh * CurrentMesh, u32 v, u32 i, u32 sn)
{
	//Now, M2MTriangles refers to M2MIndices and not to M2MVertices.
    scene::SSkinMeshBuffer *MeshBuffer = CurrentMesh->addMeshBuffer(M2MSkins[v].M2MSubmeshes[i].meshpartId);

    //Put the Indices and Vertices of the Submesh into a mesh buffer
    //Each Submesh contains only the Indices and Vertices that belong to it.
    //Because of this the Index values for the Submeshes must be corrected by the Vertex offset of the Submesh
    for(u32 j=M2MSkins[v].M2MSubmeshes[i].ofsTris;j<M2MSkins[v].M2MSubmeshes[i].ofsTris+M2MSkins[v].M2MSubmeshes[i].nTris;j++)
    {
        MeshBuffer->Indices.push_back(M2MSkins[v].M2MIndices[M2MSkins[v].M2MTriangles[j]]-M2MSkins[v].M2MSubmeshes[i].ofsVertex);
        if(M2MSkins[v].M2MIndices[M2MSkins[v].M2MTriangles[j]]<M2MSkins[v].M2MSubmeshes[i].ofsVertex)
          logerror("Index %u < ofsVertex %u",M2MSkins[v].M2MIndices[M2MSkins[v].M2MTriangles[j]],M2MSkins[v].M2MSubmeshes[i].ofsVertex);
    }

    for(u32 j=M2MSkins[v].M2MSubmeshes[i].ofsVertex;j<M2MSkins[v].M2MSubmeshes[i].ofsVertex+M2MSkins[v].M2MSubmeshes[i].nVertex;j++)
    {
        MeshBuffer->Vertices_Standard.push_back(M2Vertices[j]);
        for(u32 k=0; k<4; k++)
        {
            if((M2MVertices[j].weights[k])>0)
            {
            u32 boneIndex = M2MVertices[j].bones[k];
            scene::CM2Mesh::SWeight* weight = CurrentMesh->addWeight(CurrentMesh->getAllJoints()[boneIndex]);
            weight->strength=M2MVertices[j].weights[k];
            weight->vertex_id=MeshBuffer->Vertices_Standard.size()-1;
            weight->buffer_id=sn; 
            }

        }
    }

    MeshBuffer->recalculateBoundingBox();
    for(u32 j=0;j<M2MSkins[v].M2MTextureUnit.size();j++)//Loop through texture units
    {
        if(M2MSkins[v].M2MTextureUnit[j].submeshIndex1==i && !M2MTextureFiles[M2MTextureLookup[M2MSkins[v].M2MTextureUnit[j].textureIndex]].empty())//if a texture unit belongs to this submesh
        {
            //set vertex colors from colorblock
            //FIXME: Only the first color is used, no animation!! All animated effects should be applied through the scene node via a cm2mesh::setAnimatedEfectsForCurrentKeyframe function 
            s16 vColIndex = M2MSkins[v].M2MTextureUnit[j].colorIndex;
            if(vColIndex != -1)
            {
                DEBUG(logdebug("Applying color %f %f %f %f",M2MVertexColor[vColIndex].Colors.values[0],M2MVertexColor[vColIndex].Colors.values[1],M2MVertexColor[vColIndex].Colors.values[2],M2MVertexColor[vColIndex].Alpha.values[0]));
                video::SColor color = video::SColorf(M2MVertexColor[vColIndex].Colors.values[0],M2MVertexColor[vColIndex].Colors.values[1],M2MVertexColor[vColIndex].Colors.values[2],M2MVertexColor[vColIndex].Alpha.values[0]).toSColor();
//                 Device->getSceneManager()->getMeshManipulator()->apply(scene::SVertexColorSetManipulator(color), MeshBuffer);//
            //MeshBuffer->getMaterial().DiffuseColor = color; // if we want to set diffuse instead of vertex color
            }
            // get and set texture
            char buf[1000];
            MemoryDataHolder::MakeTextureFilename(buf,M2MTextureFiles[M2MTextureLookup[M2MSkins[v].M2MTextureUnit[j].textureIndex]].c_str());

            video::ITexture* tex = Device->getVideoDriver()->findTexture(buf);
            if(!tex)
            {
              io::IReadFile* TexFile = io::IrrCreateIReadFileBasic(Device, buf);
              if (!TexFile)
              {
                  logerror("CM2MeshFileLoader: Texture file not found: %s", buf);
                  continue;
              }
            tex = Device->getVideoDriver()->getTexture(TexFile);
            TexFile->drop();
            }
            MeshBuffer->getMaterial().setTexture(M2MSkins[v].M2MTextureUnit[j].TextureUnitNumber,tex);  // set a condition here to handle animated textures if they are used irrlicht.sourceforge.net/docu/example008.html

            DEBUG(logdebug("Render Flags: %u %u",M2MRenderFlags[M2MSkins[v].M2MTextureUnit[j].renderFlagsIndex].flags,M2MRenderFlags[M2MSkins[v].M2MTextureUnit[j].renderFlagsIndex].blending));
            u16 renderflags = M2MRenderFlags[M2MSkins[v].M2MTextureUnit[j].renderFlagsIndex].flags;

			/*
			renderflags's enumeration from some forum also found in M2Template1.bt
					RF_None = 0, 
					RF_Unlit = 1, 
					RF_Unfogged = 2,
					RF_Unlit_Unfogged = 3,
					RF_TwoSided = 4, 
					RF_Unlit_Two_Sided = 5,
					RF_Unfogged_TwoSided = 6,
					RF_Unlit_Unfogged_TwoSided = 7,
					RF_Billboard =8,
					RF_Unlit_Billboard =9,
					RF_Unfogged_Billboard =10,
					RF_Unlit_Unfogged_Billboard =11,
					RF_Billboard_TwoSided =12,
					RF_Unlit_Billboard_TwoSided =13,
					RF_Unfogged_Billboard_TwoSided =14,
					RF_Unlit_Unfogged_Billboard_TwoSided =15,
					RF_Not_ZBuffered = 16,   // 0x10
					RF_Not_ZBuffered_Unlit =17,
					RF_Not_ZBuffered_Unfogged  =18,
					RF_Not_ZBuffered_Unlit_Unfogged =19,
					RF_Not_ZBuffered_TwoSided =20,
					RF_Not_ZBuffered_Unlit_Two_Sided =21,
					RF_Not_ZBuffered_Unfogged_TwoSided =22,
					RF_Not_ZBuffered_Unlit_Unfogged_TwoSided =23,
					RF_Not_ZBuffered_Billboard =24,
					RF_Not_ZBuffered_Unlit_Billboard =25,
					RF_Not_ZBuffered_Unfogged_Billboard =26,
					RF_Not_ZBuffered_Unlit_Unfogged_Billboard =27,
					RF_Not_ZBuffered_Billboard_TwoSided =28,
					RF_Not_ZBuffered_Unlit_Billboard_TwoSided =29,
					RF_Not_ZBuffered_Unfogged_Billboard_TwoSided =30,
					RF_Not_ZBuffered_Unlit_Unfogged_Billboard_TwoSided =31
			*/
			
            MeshBuffer->getMaterial().Lighting=(renderflags & 0x01)?false:true;
            MeshBuffer->getMaterial().FogEnable=(renderflags & 0x02)?false:true;
            MeshBuffer->getMaterial().BackfaceCulling=(renderflags & 0x04)?false:true;
			//MeshBuffer->getMaterial().setFlag(video::EMF_ZBUFFER, (renderflags & 0x10)?false:true);
			MeshBuffer->getMaterial().setFlag(video::EMF_ZWRITE_ENABLE, (renderflags & 0x10)?false:true);
            //We have a problem here      zwrite or ztest?
            //             MeshBuffer->getMaterial().ZBuffer=(renderflags & 0x10)?video::ECFN_LESS:video::ECFN_LESSEQUAL;

            switch(M2MRenderFlags[M2MSkins[v].M2MTextureUnit[j].renderFlagsIndex].blending)
            {
              case 0:  //opaque
                MeshBuffer->getMaterial().MaterialType=video::EMT_SOLID;
                break;
              case 1:  //alpha ref
                MeshBuffer->getMaterial().MaterialType=video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF;  // default REF = 127 maybe set a lower REF?
                break;
              case 2:  //alpha blend
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
				MeshBuffer->getMaterial().MaterialTypeParam = pack_texureBlendFunc(video::EBF_SRC_ALPHA, video::EBF_ONE_MINUS_SRC_ALPHA,  video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
				break;
              case 3:  //additive
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_texureBlendFunc(video::EBF_SRC_COLOR, video::EBF_DST_COLOR, video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
				break;
              case 4:  //additive alpha
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
				MeshBuffer->getMaterial().MaterialTypeParam = pack_texureBlendFunc(video::EBF_SRC_ALPHA, video::EBF_ONE, video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
				break;
              case 5:  //modulate blend
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
				MeshBuffer->getMaterial().MaterialTypeParam = pack_texureBlendFunc(video::EBF_ONE, video::EBF_SRC_COLOR, video::EMFN_MODULATE_1X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
				break;
              case 6:  //Lumirion: not sure exatly so I'm using modulate2x blend like wowmodelviewer or could use EMT_TRANSPARENT_ADD_COLOR
                MeshBuffer->getMaterial().MaterialType=video::EMT_ONETEXTURE_BLEND;
                MeshBuffer->getMaterial().MaterialTypeParam = pack_texureBlendFunc(video::EBF_DST_COLOR, video::EBF_SRC_COLOR, video::EMFN_MODULATE_2X, video::EAS_TEXTURE|video::EAS_VERTEX_COLOR);
				break;
            }
        }

    }

    MeshBuffer->recalculateBoundingBox();
    if(header.Animations.num==0 && header.GlobalSequences.num == 0)
      MeshBuffer->setHardwareMappingHint(EHM_STATIC);
    else
      MeshBuffer->setHardwareMappingHint(EHM_STREAM);

}


bool CM2MeshFileLoader::load()
{
  DEBUG(logdebug("Trying to open file %s",MeshFile->getFileName().c_str()));

  MeshFile->read(&header,20);
  DEBUG(logdebug("M2 Version %X",header.version));

  switch(header.version)
  {
    case 0x100:
    case 0x104://NEED CHECK
    case 0x105://NEED CHECK
    case 0x106://NEED CHECK
    case 0x107://NEED CHECK
    {
      MeshFile->read((u8*)&header+20,sizeof(ModelHeader)-20);
      MeshFile->seek(header.Views.ofs);
      MeshFile->read(&currentView,sizeof(ModelView));
      ReadViewData(MeshFile);

      break;
    }
    case 0x108:
    {
      //This is pretty ugly... Any suggestions how to make this nicer?
      MeshFile->read((u8*)&header+0x14,24);//nGlobalSequences - ofsAnimationLookup
      MeshFile->read((u8*)&header+0x34,28);//nBones - nViews
      MeshFile->read((u8*)&header+0x54,24);//nColors - nTransparency
      MeshFile->read((u8*)&header+0x74,sizeof(ModelHeader)-0x74);//nTexAnims - END

      //std::string SkinName = MeshFile->getFileName().c_str();
      //SkinName = SkinName.substr(0, SkinName.length()-3) + "00.skin"; // FIX ME if we need more skins
	
	  // Load and read skins
	  for (u16 n = 0; n < header.Views.num; n++)
	  {
			std::string SkinName = MeshFile->getFileName().c_str();
			//SkinName = SkinName.substr(0, SkinName.length()-3) + "00.skin"; // FIX ME if we need more skins
			SkinName = SkinName.substr(0, SkinName.length()-3);
    
			// make up a skin name
			std::ostringstream ext;
			ext<<SkinName;
			if (n<10)
			{
				ext<< "0";
				ext<< n;
				ext<< ".skin";
			}
			if (n>9)
			{
				ext<< n;
				ext<< ".skin";
			}
			SkinName = ext.str();
    
			// Load our made up filename.  
			io::IReadFile* SkinFile = io::IrrCreateIReadFileBasic(Device, SkinName.c_str()); // if it is there load it
			if (!SkinFile) // if it didnt load we have an error
			{
				logerror("Error! Skin file not found: %s", SkinName.c_str());
				return 0;
			}
			else           // No error so read the skin
			{
				SkinFile->seek(4); // Header of Skin Files is always SKIN
				SkinFile->read(&currentView,sizeof(ModelView)); // overwrites previous currentview if any
				ReadViewData(SkinFile);
				SkinFile->drop();
				logdetail("Read veiw data from %s .", SkinName.c_str());
			}
	  }

      break;
    }
    default:
    {
      logerror("M2: [%s] Wrong header %0X! File version doesn't match or file is not a M2 file.",MeshFile->getFileName().c_str(),header.version);
      return 0;
    }
  }
  ReadVertices();

  ReadTextureDefinitions();
  ReadAnimationData();

  ReadBones();
  ReadColors();

///////////////////////////
// EVERYTHING IS READ
///////////////////////////
  
//////////////////////////////////////////////////
// Colect and store submesh data for all submeshes
//////////////////////////////////////////////////


// ToDo:: Store all M2MTextureFiles in the CM2Mesh.  Copy all M2MSkins to AnimatedMesh.Skins. Store all Animated data in the CM2Mesh too
for(u16 S = 0; S < M2MSkins.size(); S++)
{
	CM2Mesh::skin skin;
	core::array<CM2Mesh::submesh> sky;
	core::array<CM2Mesh::submesh> scene;
	core::array<Bounds> SubmeshBounds; // boundry height/width for decal sorting
	
	AnimatedMesh->Skins.push_back(skin); // add an empty skin
	AnimatedMesh->Skins[S].ID = S ; // 00.skin will be Skins[0]
	for(u16 s = 0; s < M2MSkins[S].M2MSubmeshes.size(); s++)
	{
		CM2Mesh::submesh Submesh;  
		//AnimatedMesh->Skins[S].Submeshes.push_back(Submesh); // store a blank submesh
		Submesh.MeshPart = M2MSkins[S].M2MSubmeshes[s].meshpartId; // AnimatedMesh->Skins[S].Submeshes.getLast().
		// Store parent bone for submesh so we can link submesh to bone
		Submesh.RootBone = M2MBoneLookupTable[M2MSkins[S].M2MSubmeshes[s].unk4]; // M2MSkins[S].M2MSubmeshes[s].unk4;
		Submesh.Radius = M2MSkins[S].M2MSubmeshes[s].Radius;
		Submesh.LoaderIndex = s; // save an index to this submesh's current location so if this element is reordered we can still access its data in this loader
		// vertex range should be the same for all instances of a submesh regardless of current skin so 
		// using it to name submeshes is a good way to prevent duplication while using multiple skins
		std::ostringstream vertrange;
		vertrange<< Device->getFileSystem()->getFileBasename(MeshFile->getFileName().c_str(), false).c_str();
		vertrange<< M2MSkins[S].M2MSubmeshes[s].ofsVertex;
		vertrange<< "_";
		u16 endvertrange = M2MSkins[S].M2MSubmeshes[s].ofsVertex+M2MSkins[S].M2MSubmeshes[s].nVertex;
		vertrange<< endvertrange;
		Submesh.UniqueName = vertrange.str().c_str(); // save the submesh name, later when switching skins we check by this name if this submesh should be visible we reuse it updating its data for the current skin
		
		// get distance from camera
		float closestDistance = 100000000.0f; //Arbitraty large number
		core::vector3df pos(11.11f,2.44f,-0.03f); //The position of the camera. To compare with.
		u16 nearestvertex;
		for (int j = M2MSkins[S].M2MSubmeshes[s].ofsVertex;j < M2MSkins[S].M2MSubmeshes[s].ofsVertex + M2MSkins[S].M2MSubmeshes[s].nVertex; j++) // loop through vertexes for this submesh
		{
			core::vector3df v = M2MVertices[j].pos - pos;
			float dist = (v.X * v.X) + (v.Y * v.Y) + (v.Z * v.Z); //Squared distance
			if (dist < closestDistance)
			{
				closestDistance = dist;
				nearestvertex = j;
			}
		}
		Submesh.Distance = closestDistance; // distance between camera and closest vertex in submesh
		Submesh.NearestVertex = nearestvertex - M2MSkins[S].M2MSubmeshes[s].ofsVertex; // index to nearest vertex in submesh's vertices 


		// get submesh bounds for this submesh
		Bounds tempBounds; 
		core::vector3df center = fixCoordSystem(M2MSkins[S].M2MSubmeshes[s].CenterOfMass);
		// Get edges
		tempBounds.Wmax = center.X;
		tempBounds.Wmin = center.X;
		tempBounds.Hmax = center.Y;
		tempBounds.Hmin = center.Y;
		for (int j = M2MSkins[S].M2MSubmeshes[s].ofsVertex; j < M2MSkins[S].M2MSubmeshes[s].ofsVertex + M2MSkins[S].M2MSubmeshes[s].nVertex-1; j++)
		{
			// X
			if (M2MVertices[j].pos.X < tempBounds.Wmin)
			{
				tempBounds.Wmin = M2MVertices[j].pos.X;
			}
			if (M2MVertices[j].pos.X > tempBounds.Wmax)
			{
				tempBounds.Wmax = M2MVertices[j].pos.X;
			}
			// Y
			if (M2MVertices[j].pos.Y < tempBounds.Hmin)
			{
				tempBounds.Hmin = M2MVertices[j].pos.Y;
			}
			if (M2MVertices[j].pos.Y > tempBounds.Hmax)
			{
				tempBounds.Hmax = M2MVertices[j].pos.Y;
			}
		}
		SubmeshBounds.push_back(tempBounds);  // ToDo:: compensate for the camera not looking down the z axis.  generate a rotation that will make z axis aline with camera axis without damaging geomatry 
		                                      // and apply it to each value in tempBounds to make decal positioning easyer to think about

				
		// get all texture data for this submesh
		for (u16 t = 0; t < M2MSkins[S].M2MTextureUnit.size(); t++)
		{
			if (M2MSkins[S].M2MTextureUnit[t].submeshIndex1 == s) // Big S = current skin index little s= current submesh index
			{
				scene::CM2Mesh::texture Texture;
				Texture.TextureNumber = M2MSkins[S].M2MTextureUnit[t].TextureUnitNumber;
				Texture.Path = M2MTextureLookup[M2MSkins[S].M2MTextureUnit[t].textureIndex]; 
				u8 animatedFlag = M2MSkins[S].M2MTextureUnit[t].Flags & 0xFF;   // get first 8bits this flag sets texture animation
				if (animatedFlag == 0){
					Texture.animated = true;}
				else{
					Texture.animated = false;}
				Texture.shaderType = M2MSkins[S].M2MTextureUnit[t].renderOrder;
				Texture.RenderFlag = M2MRenderFlags[M2MSkins[S].M2MTextureUnit[t].renderFlagsIndex].flags;
				Texture.BlendFlag = M2MRenderFlags[M2MSkins[S].M2MTextureUnit[t].renderFlagsIndex].blending;
				Texture.Mode = M2MSkins[S].M2MTextureUnit[t].Mode;
				u8 renderFlag = M2MSkins[S].M2MTextureUnit[t].Flags >> 8;
				Texture.Block = renderFlag;
				Texture.VertexColor = M2MSkins[S].M2MTextureUnit[t].colorIndex;
				Texture.transparency = M2MSkins[S].M2MTextureUnit[t].transparencyIndex;
				Texture.uvanimation = M2MSkins[S].M2MTextureUnit[t].texAnimIndex;
						
				Submesh.Textures.push_back(Texture);
			}
		}
		// ToDo:: only do this sorting stuf for scenes
		// Filter submeshes into groups by size.
		if (Submesh.Radius >= 70) // if it is a large submeshs its part of the sky
		{
			sky.push_back(Submesh);                  
		}
		if (Submesh.Radius < 70) // if this submesh isn't to big its a normal scene element 
		{
			scene.push_back(Submesh);
		}
	}
	FixDecalDistance(scene, SubmeshBounds);
	FixDecalDistance(sky, SubmeshBounds);
	sortDistance(sky);
	sortDistance(scene);
	for (u16 t = 0; t <sky.size()-1; t++){
		if (t>0){sky[t].Distance = sky[0].Distance-t;} // distance ofset so that sequence remains correct if resorted later
		AnimatedMesh->Skins[S].Submeshes.push_back(sky[t]);} // store data for skin/view in mesh
	for (u16 t = 0; t <scene.size()-1; t++){
		AnimatedMesh->Skins[S].Submeshes.push_back(scene[t]);}
	sky.clear();
	scene.clear();
}

// set default skin for the mesh
AnimatedMesh->SkinID = 0;
// Copy global data lists to the mesh
for (u16 t = 0; t <M2MTextureFiles.size(); t++)
{
	AnimatedMesh->Textures.push_back(M2MTextureFiles[t].c_str());
}
//ToDo:: colors lights cams other animation etc



/////////////////////////////////////////////////
//     submesh maping to reorder submeshes     //
/////////////////////////////////////////////////
    // this section will be depreciated when CM2Mesh stores it's own skins
	scene::CM2Mesh::BufferInfo meshmap;
	irr::core::array<scene::CM2Mesh::BufferInfo> SubmeshMap;

	u16 skin = AnimatedMesh->SkinID;

	u32 range = M2MSkins[skin].M2MSubmeshes.size();
	for (u16 s = 0; s < range; s++)           // Loop through the submeshes
	{
		u16 t;
		for (u16 T = 0; T < M2MSkins[skin].M2MTextureUnit.size(); T++){     // Find the first textureunit that applies to submesh S
			if (M2MSkins[skin].M2MTextureUnit[T].submeshIndex1 == s){
				t=T;
				T=M2MSkins[skin].M2MTextureUnit.size(); // End the loop since we found a texture
			}
		}
		meshmap.ID = s;                                     // This SubMesh's index
		meshmap.Mode = M2MSkins[skin].M2MTextureUnit[t].Mode;              // This is mode.
		u8 animatedFlag = M2MSkins[skin].M2MTextureUnit[t].Flags & 0xFF;   // get first 8bits this flag sets texture animation
		u8 renderFlag = M2MSkins[skin].M2MTextureUnit[t].Flags >> 8;       // get the second 8bits this flag sets some sort of submesh grouping
		meshmap.order = M2MSkins[skin].M2MTextureUnit[t].renderOrder;      // Indicates how this effect should be applyed for instance 2 is an overlay
		meshmap.block = renderFlag;
		meshmap.Coordinates = fixCoordSystem(M2MSkins[skin].M2MSubmeshes[meshmap.ID].CenterOfMass);// fix the coordinates since Y is depth in ModelViewSubmesh vectors but we use Z depth 
		meshmap.flag = M2MRenderFlags[M2MSkins[skin].M2MTextureUnit[t].renderFlagsIndex].flags;
		meshmap.blend = M2MRenderFlags[M2MSkins[skin].M2MTextureUnit[t].renderFlagsIndex].blending;
		if (meshmap.blend > 1)
			meshmap.solid = false;
		else
			meshmap.solid = true;
		if (animatedFlag == 0)                             //is this an animated texture? Also may need to include the index to the texture animation (is it the same as the ID).
			meshmap.animatedtexture = true;
		else
			meshmap.animatedtexture = false;
		// trying to understand what this flag is
		meshmap.unknown = M2MSkins[skin].M2MSubmeshes[meshmap.ID].unk3;
		std::string tex = Device->getFileSystem()->getFileBasename(M2MTextureFiles[M2MTextureLookup[M2MSkins[skin].M2MTextureUnit[t].textureIndex]].c_str(), false).c_str();
		meshmap.Radius = M2MSkins[skin].M2MSubmeshes[meshmap.ID].Radius;

		logdetail("For Submesh %u unk3 %u and unk4 %u texture", meshmap.ID, meshmap.unknown, M2MSkins[skin].M2MSubmeshes[meshmap.ID].unk4); // %s.", meshmap.ID, meshmap.unknown, M2MSkins[skin].M2MSubmeshes[meshmap.ID].unk4, tex);
		logdetail(tex.c_str());
		logdetail("  mode %u block %u Order %u", meshmap.Mode, meshmap.block, meshmap.order);
		std::ostringstream rad;
		rad<< meshmap.Radius;
		std::string Radius = rad.str();
		logdetail("    RenderFlag %u Radius = %s", meshmap.flag, Radius.c_str());

		float B = M2MVertices[M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex].pos.Z; 
		float F = B;
		logdetail("Start vertex %u end vertex %u", M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex, M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex + M2MSkins[skin].M2MSubmeshes[meshmap.ID].nVertex);
		/*for (int j = M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex; j < M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex + M2MSkins[skin].M2MSubmeshes[meshmap.ID].nVertex; j++)
		{ 
			/* // if total vertices are less than the largest index into vertices for this submesh
			if (M2MVertices.size()-1 < M2MSkins[skin].M2MSubmeshes.getLast().ofsVertex + M2MSkins[skin].M2MSubmeshes.getLast().nVertex)
			{
				// M2MVertices has less elements than are referenced in the skins for ui northrend
				// add empty vertices at the end of m2mvertices to prevent core::array acces violation when direct accessing the last element in array for debug build
				for (u16 q = M2MVertices.size()-1; q < M2MSkins[skin].M2MSubmeshes.getLast().ofsVertex + M2MSkins[skin].M2MSubmeshes.getLast().nVertex; q++)
				{
				M2MVertices.push_back(M2MVertices.getLast()); // coppy the last ellement untill total ellements = total refrences
				}
			}*/
			/*if (M2MVertices[j].pos.Z > B)  // find the farthest(largest) depth value in the submesh
			{
				B = M2MVertices[j].pos.Z;
			}
			if (M2MVertices[j].pos.Z < F) // find the nearest(Smallest) depth value in the submesh
			{
				F = M2MVertices[j].pos.Z;
			}*/
		//}
		meshmap.Back = B;
		meshmap.Front = F;
		meshmap.Middle = meshmap.Coordinates.Z; // middle value for aabb
		// Get X edges
		meshmap.Xpos = meshmap.Coordinates.X;
		meshmap.Xneg = meshmap.Coordinates.X;
		/*for (int j = M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex; j < M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex + M2MSkins[skin].M2MSubmeshes[meshmap.ID].nVertex; j++)
		{
			if (M2MVertices[j].pos.X < meshmap.Xneg)
			{
				meshmap.Xneg = M2MVertices[j].pos.X;
			}
			if (M2MVertices[j].pos.X > meshmap.Xpos)
			{
				meshmap.Xpos = M2MVertices[j].pos.X;
			}
		}*/
		// Get Y edges
		meshmap.Ypos = meshmap.Coordinates.Y;
		meshmap.Yneg = meshmap.Coordinates.Y;
		/*for (int j = M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex; j < M2MSkins[skin].M2MSubmeshes[meshmap.ID].ofsVertex + M2MSkins[skin].M2MSubmeshes[meshmap.ID].nVertex; j++)
		{
			if (M2MVertices[j].pos.Y < meshmap.Yneg)
			{
				meshmap.Yneg = M2MVertices[j].pos.Y;
			}
			if (M2MVertices[j].pos.Y > meshmap.Ypos)
			{
				meshmap.Ypos = M2MVertices[j].pos.Y;
			}
		}*/
		// Set the Sort Point (currently using mode might be beater to use renderflag to determine sortpoint)
		    //Mode 1 meshes
		if (meshmap.Mode == 1){                         // Mountains and backdrop
			meshmap.SortPoint = meshmap.Back;}          //farthest edge
		if (meshmap.Mode == 1 && meshmap.Radius >= 2 && meshmap.Radius <= 6){     // Walls etc
			meshmap.SortPoint = meshmap.Front;}         //near edge
			//Mode 2 meshes
		if (meshmap.Mode == 2 && meshmap.Radius > 50){  // Backdrop meshes
			meshmap.SortPoint = meshmap.Back;}          //far edge though it comes out the same in northrend ui using middle or near
		if (meshmap.Mode == 2 && meshmap.Radius > 10 && meshmap.Radius < 50){  // Large efects like light rays
			meshmap.SortPoint = meshmap.Front;}         //Switch to far edge?
		if (meshmap.Mode == 2 && meshmap.Radius >=2 && meshmap.Radius < 10){   // Medium None order2 effect meshes 
			meshmap.SortPoint = meshmap.Front;}         //near edge
		if (meshmap.Mode == 2 && meshmap.Radius < 2){   // Small None order2 effect meshes 
			meshmap.SortPoint = meshmap.Middle;}        //should use their middle
			//Order 2 meshes                            //Actualy the near edge might realy work better
		if (meshmap.order == 2 && meshmap.Radius > 50){ // If an overlay is in the backdrop we need to use the middle value to interact with big meshes
			meshmap.SortPoint = meshmap.Middle;}        //Middle point ( compaird to active edge)
		if (meshmap.order == 2 && meshmap.Radius < 50){ // In the rest of the mesh we need to use the back edge.
			meshmap.SortPoint = meshmap.Back;}          //back edge
			
		// Put the Populated Element into the Array
		SubmeshMap.push_back(meshmap);
	}
	// copy elements to BufferMap
	/* // Backdrop
	core::array<scene::CM2Mesh::BufferInfo> Scene;
	core::array<scene::CM2Mesh::BufferInfo> Overlays;
	for (u16 b = 0; b < SubmeshMap.size(); b++)
	{                                      // ToDo:: this filter is leaving large order 2 elements where they fall in backdrop. Overlay handling when finished will handle both size ranges of overlay
		if (SubmeshMap[b].Radius > 60){                              // Backdrop
			Scene.push_back(SubmeshMap[b]);}
		if (SubmeshMap[b].Radius < 60 && SubmeshMap[b].order == 2){  // Might as well collect the over lays here
			Overlays.push_back(SubmeshMap[b]);}
	}
	// Sort by SortPoint farthest to nearest
	sortPointHighLow(Scene); 
	// Now copy Backdrops into the mesh's BufferMap
	for (u16 b = 0; b < Scene.size(); b++){
		AnimatedMesh->BufferMap.push_back(Scene[b]);}
	// Empty our Temporary Arrays
	Scene.erase(0,Scene.size());
	
	// Middle 
	for (u16 b = 0; b < SubmeshMap.size(); b++)
	{
		// get submeshes whose radius is not insainly large and block is less than 10   && SubmeshMap[b].order != 2 
		if (SubmeshMap[b].Radius < 60 && SubmeshMap[b].solid == false && SubmeshMap[b].block < 10 && SubmeshMap[b].order !=2){
			Scene.push_back(SubmeshMap[b]);
		}
	}
	// sort them
	sortPointHighLow(Scene);
	// store them
	for (u16 b = 0; b < Scene.size(); b++){
		AnimatedMesh->BufferMap.push_back(Scene[b]);}
	// Empty our temporary arrays
	Scene.erase(0,Scene.size());
	
	// Solids.  //Put solids here Since I have a natural break here between forground and the rest of the scene? (Not nesasary to process them seperately but keeps the dragon out from behind the lightray beneath it so it doesnt shine white)
	for (u16 b = 0; b < SubmeshMap.size(); b++)
	{
		// solids
		if (SubmeshMap[b].Radius < 50 && SubmeshMap[b].solid == true){
			Scene.push_back(SubmeshMap[b]);
		}
	}
	sortPointHighLow(Scene);
	// copy to BufferMap
	for (u16 b = 0; b < Scene.size(); b++){
		AnimatedMesh->BufferMap.push_back(Scene[b]);}
	// Empty our temporary arrays
	Scene.erase(0,Scene.size());
	
	// ForeGround.   //Get transparent submeshes whose radius is not insainly large and block is greater than or equal to 10
	for (u16 b = 0; b < SubmeshMap.size(); b++)
	{
		if (SubmeshMap[b].Radius < 60 && SubmeshMap[b].solid == false && SubmeshMap[b].block >= 10 && SubmeshMap[b].order !=2){
			Scene.push_back(SubmeshMap[b]);
		}
	}
	// sort them
	sortPointHighLow(Scene);
	// copy them to the BufferMap
	for (u16 b = 0; b < Scene.size(); b++){
		AnimatedMesh->BufferMap.push_back(Scene[b]);}
	// Empty our temporary arrays
	Scene.erase(0,Scene.size());
	
	// Now insert overlays
	//InsertOverlays(Overlays,AnimatedMesh->BufferMap);
	*/
	for (u16 b = 0; b < SubmeshMap.size(); b++){
		AnimatedMesh->BufferMap.push_back(SubmeshMap[b]);} /*
	sortRadius (0, SubmeshMap.size()-1, 0);
	sortSizeBracketByMode (0, strt-1, 0);
	sortModeByBlock(0, strt-1, 0);
	sortSizeBracketByMode (strt, SubmeshMap.size()-1, 0);
	sortModeByBlock(strt, SubmeshMap.size()-1, 0);*/

	
	// Clear Temps
	//Overlays.clear();
	//Scene.clear();
	SubmeshMap.clear();


///////////////////////////////////////
//           Fix Geometry            //
///////////////////////////////////////

//M2MVertices are not usable like this. Thus we put it into an irrlicht S3DVertex

if(M2Vertices.size()>0)
    M2Vertices.clear();

for(u32 i=0;i<M2MVertices.size();i++)
{
    M2Vertices.push_back(video::S3DVertex(core::vector3df(M2MVertices[i].pos.X,M2MVertices[i].pos.Y,M2MVertices[i].pos.Z),core::vector3df(M2MVertices[i].normal.X,M2MVertices[i].normal.Y,M2MVertices[i].normal.Z), video::SColor(255,100,100,100),M2MVertices[i].texcoords));
}


///////////////////////////////////////
//      Animation related stuff      //
///////////////////////////////////////
	

CopyAnimationsToMesh(AnimatedMesh); 


///////////////////////////////////////
//           Mesh Assembly           //
///////////////////////////////////////

//Loop through the submeshes  // ToDo:: keep track of triangle offsets and number so we don't duplicate submeshes that exist in multiple views/.skins and store a simplified texture list in the cm2mesh so we can swap textures by currentview flag
std::string prefix = "bob"; //"UI_";
std::string MeshFileName = Device->getFileSystem()->getFileBasename(MeshFile->getFileName().c_str(), false).c_str();

if (std::mismatch(prefix.begin(), prefix.end(), MeshFileName.begin()).first == prefix.end()) // If MeshFileName begins with UI_
//if ( strstr( MeshFileName.c_str(), "UI_" )) // if MeshFileName contains UI_
{
	u32 v = 0; // currently I am limiting skin usage to skin 0
	AnimatedMesh->SkinID = v; // remove this later as skinid is set when storing globals in the mesh
	//for (u32 i = 0; i < M2MSkins[v].M2MSubmeshes.size(); i++)
	for (u32 i = 0; i < AnimatedMesh->Skins[v].Submeshes.size(); i++)
	{
		// will need to see if a mesh with this UniqueName is already loaded to avoid multiple copies
		CM2Mesh *CurrentChildMesh = new scene::CM2Mesh(); // make a blank mesh
		CopyAnimationsToMesh(CurrentChildMesh);
		//BuildANewSubMesh(CurrentChildMesh, v, i); // assemble childmesh
		BuildANewSubMesh(CurrentChildMesh, v, AnimatedMesh->Skins[v].Submeshes[i].LoaderIndex, i); // assemble childmesh
		Device->getSceneManager()->getMeshManipulator()->flipSurfaces(CurrentChildMesh);
		
		// vertex range should be the same for all instances of a submesh regardless of current skin so 
		// using it to name submeshes is a good way to prevent duplication while using multiple skins
		// rename it with a unique name.  UniqueName has already ben generated and stored in the CM2Mesh instance
		//std::string NewName = AnimatedMesh->Skins[v].Submeshes[i].UniqueName.c_str();

		Device->getSceneManager()->getMeshCache()->renameMesh(CurrentChildMesh, AnimatedMesh->Skins[v].Submeshes[i].UniqueName);//NewName.c_str()); // give the current child mesh a uniqe name
		CurrentChildMesh->finalize(); // do this after this submesh is fully generated
		// Do I need to add this mesh to meshcache?
		Device->getSceneManager()->getMeshCache()->addMesh(AnimatedMesh->Skins[v].Submeshes[i].UniqueName, CurrentChildMesh); //NewName.c_str()
	}
}
else
{
	u32 v = 0;
	//for (u32 i = 0; i < M2MSkins[v].M2MSubmeshes.size(); i++)
	for (u32 i = 0; i < AnimatedMesh->Skins[v].Submeshes.size(); i++)
	{
		BuildANewSubMesh(AnimatedMesh, v, AnimatedMesh->Skins[v].Submeshes[i].LoaderIndex, i); //i);
	}
	Device->getSceneManager()->getMeshManipulator()->flipSurfaces(AnimatedMesh);
}


///////////////////////////////////////
//             Clean Up              //
///////////////////////////////////////


for(u32 i=0; i< M2MAnimfiles.size();i++)
{
  if(M2MAnimfiles[i]!=0)
    M2MAnimfiles[i]->drop();
}

M2MAnimations.clear();
M2MAnimfiles.clear();
M2MTriangles.clear();
M2Vertices.clear();
M2Indices.clear();
M2MIndices.clear();
M2MVertices.clear();
M2MRenderFlags.clear();
M2MTextureUnit.clear();
M2MTextureDef.clear();
M2MSubmeshes.clear();
M2MTextureFiles.clear();
M2MTextureLookup.clear();
M2MVertexColor.clear();
M2MLights.clear();
M2MCameras.clear();
M2MUVAnimations.clear();
return true;
}

}
}
