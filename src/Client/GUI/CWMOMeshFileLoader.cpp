#include <iostream>
#include <cstdarg>
#include "MemoryDataHolder.h"
#include "MemoryInterface.h"
#include "CWMOMeshFileLoader.h"
#include "common.h"

inline void flipcc(irr::u8 *fcc)
{
    char t;
    t=fcc[0];
    fcc[0]=fcc[3];
    fcc[3]=t;
    t=fcc[1];
    fcc[1]=fcc[2];
    fcc[2]=t;
}



namespace irr
{
namespace scene
{

CWMOMeshFileLoader::CWMOMeshFileLoader(IrrlichtDevice* device):Device(device)
{
    Mesh = NULL;

}

CWMOMeshFileLoader::~CWMOMeshFileLoader()
{

}

bool CWMOMeshFileLoader::isALoadableFileExtension(const io::path& filename)const
{
    return core::hasFileExtension ( filename, "wmo" );
}


//! creates/loads an animated mesh from the file.
//! \return Pointer to the created mesh. Returns 0 if loading failed.
//! If you no longer need the mesh, you should call IAnimatedMesh::drop().
//! See IUnknown::drop() for more information.
IAnimatedMesh* CWMOMeshFileLoader::createMesh(io::IReadFile* file)
{
    if(!file)
        return 0;
    MeshFile = file;
    std::string filename=MeshFile->getFileName().c_str();
    Mesh = new scene::CM2Mesh();

	if ( load(true) )//We try loading a root file first!
    {
		for(u32 i=0;i<rootHeader.nGroups;i++)//On success, load all group files. This is getting slow as molasses for large files like Stormwind.wmo
        {
            char grpfilename[255];
            sprintf(grpfilename,"%s_%03u.wmo",filename.substr(0,filename.length()-4).c_str(),i);
            DEBUG(logdev("%s",grpfilename));
            MeshFile = io::IrrCreateIReadFileBasic(Device,grpfilename);
            if(!MeshFile)
            {
                logerror("Could not read file %s!",grpfilename);
                return 0;
            }
            load(false);
        }
    Mesh->updateBoundingBox();
    Device->getSceneManager()->getMeshManipulator()->flipSurfaces(Mesh); //Fix inverted surfaces after the rotation
    //Does this crash on windows?
    Device->getSceneManager()->getMeshManipulator()->recalculateNormals(Mesh,true);//just to be sure
    DEBUG(logdev("Complete Mesh contains a total of %u submeshes!",Mesh->getMeshBufferCount()));
	}
	else
	{
		Mesh->drop();
		Mesh = 0;
	}

	return Mesh;
}
bool CWMOMeshFileLoader::load(bool _root)
{
    bool isRootFile = _root;
    u8 _cc[5];
    u8 *fourcc = &_cc[0];
    fourcc[4]=0;
    u32 size;
    u32 textureOffset;

DEBUG(logdev("Trying to open file %s",MeshFile->getFileName()));

while(MeshFile->getPos() < MeshFile->getSize())
{
MeshFile->read(fourcc,4);
MeshFile->read(&size,4);
flipcc(fourcc);
DEBUG(logdev("Reading Chunk: %s size %u", (char*)fourcc,size));

     if(!strcmp((char*)fourcc,"MVER")){
        DEBUG(logdev("MVER Chunk: %s",(char*)fourcc));
        MeshFile->seek(size,true);
     }

    //Start root file parsing
     else if(!strcmp((char*)fourcc,"MOHD")){
        MeshFile->read(&rootHeader,sizeof(RootHeader));
        DEBUG(logdev("Read Root Header: %u Textures, %u Groups, %u Models", rootHeader.nTextures, rootHeader.nGroups, rootHeader.nModels));
        if(!isRootFile)//We should be reading a group file and found a root header, abort
            return 0;
     }
     else if(!strcmp((char*)fourcc,"MOTX")){
        textureOffset=MeshFile->getPos();
        MeshFile->seek(size,true);
     }
     else if(!strcmp((char*)fourcc,"MOMT")){
        if(WMOMTexDefinition.size()>0)
            WMOMTexDefinition.clear();

        MOMT_Data tempMOMT;
        for(u32 i =0;i<(size/sizeof(MOMT_Data));i++)
        {
            MeshFile->read(&tempMOMT,sizeof(MOMT_Data));
            WMOMTexDefinition.push_back(tempMOMT);
        }
        DEBUG(logdev("Read %u/%u TextureDefinitions",WMOMTexDefinition.size(),(size/sizeof(MOMT_Data))));

        u32 tempOffset = MeshFile->getPos();//Save current position for further reading until texture file names are read.

        if(WMOMTextureFiles.size()>0)
            WMOMTextureFiles.clear();
        std::string tempTexName;
	u32 texNameSize;

        for(u32 i =0;i<WMOMTexDefinition.size();i++)
        {
        texNameSize = WMOMTexDefinition[i].endNameIndex-WMOMTexDefinition[i].startNameIndex; tempTexName.resize(texNameSize);
        MeshFile->seek(textureOffset+WMOMTexDefinition[i].startNameIndex);
        MeshFile->read((void*)tempTexName.c_str(),WMOMTexDefinition[i].endNameIndex-WMOMTexDefinition[i].startNameIndex);
        DEBUG(logdev("Texture %u: %s",i,tempTexName.c_str()));
        WMOMTextureFiles.push_back(tempTexName.c_str());
        }

        MeshFile->seek(tempOffset);
     }



     //Start Group file parsing
     else if(!strcmp((char*)fourcc,"MOGP")){
        DEBUG(logdev("header okay: %s",(char*)fourcc));
        MeshFile->seek(68,true);
        if(isRootFile)//We should be reading a root file and found a Group header, abort
            return 0;
     }
     else if(!strcmp((char*)fourcc,"MOPY")){//Texturing information (1 per triangle);
        if(WMOMTexData.size()>0)
            WMOMTexData.clear();
        submeshes.clear();//Saves last vertex of submes
        u16 previous_texid=999;//outside u8 space
        MOPY_Data tempMOPY;
        for(u32 i =0;i<(size/sizeof(MOPY_Data));i++)
        {
            MeshFile->read(&tempMOPY,sizeof(MOPY_Data));
            if(previous_texid==999)
                previous_texid=tempMOPY.textureID;//Initialize
            WMOMTexData.push_back(tempMOPY);
            if(previous_texid!=tempMOPY.textureID)
                submeshes.push_back(i);
            previous_texid=tempMOPY.textureID;
        }
            submeshes.push_back(WMOMTexData.size()-1);//last read entry
        DEBUG(logdev("Read %u/%u Texture Informations, counted %u submeshes",WMOMTexData.size(),(size/sizeof(MOPY_Data)),submeshes.size()));

     }
     else if(!strcmp((char*)fourcc,"MOVI")){//Vertex indices (3 per triangle)
        if(WMOMIndices.size()>0)
            WMOMIndices.clear();

        u16 tempWMOIndex;
        for(u32 i =0;i<(size/sizeof(u16));i++)
        {
            MeshFile->read(&tempWMOIndex,sizeof(u16));
            WMOMIndices.push_back(tempWMOIndex);
        }
        DEBUG(logdev("Read %u/%u Indices",WMOMIndices.size(),(size/sizeof(u16))));

     }
     else if(!strcmp((char*)fourcc,"MOVT")){//Vertex coordinates
        if(WMOMVertices.size()>0)
            WMOMVertices.clear();

        core::vector3df tempWMOVertex;
        float tempYZ;
        for(u32 i =0;i<(size/sizeof(core::vector3df));i++)
        {
            MeshFile->read(&tempWMOVertex,sizeof(core::vector3df));
            tempYZ = tempWMOVertex.Y;
            tempWMOVertex.Y=tempWMOVertex.Z;
            tempWMOVertex.Z=tempYZ;
            WMOMVertices.push_back(tempWMOVertex);
        }
        DEBUG(logdev("Read %u/%u Vertex Coordinates",WMOMVertices.size(),(size/sizeof(core::vector3df))));

     }
    else if(!strcmp((char*)fourcc,"MONR")){//Normals
        if(WMOMNormals.size()>0)
            WMOMNormals.clear();

        core::vector3df tempWMONormal;
        float tempYZ;
        for(u32 i =0;i<(size/sizeof(core::vector3df));i++)
        {
            MeshFile->read(&tempWMONormal,sizeof(core::vector3df));
            tempYZ = tempWMONormal.Y;
            tempWMONormal.Y=tempWMONormal.Z;
            tempWMONormal.Z=tempYZ;
            WMOMNormals.push_back(tempWMONormal);
        }
        DEBUG(logdev("Read %u/%u Normal Coordinates",WMOMNormals.size(),(size/sizeof(core::vector3df))));

     }
    else if(!strcmp((char*)fourcc,"MOTV")){//TexCoord
        if(WMOMTexcoord.size()>0)
            WMOMTexcoord.clear();

        core::vector2df tempWMOMTexcoord;
        for(u32 i =0;i<(size/sizeof(core::vector2df));i++)
        {
            MeshFile->read(&tempWMOMTexcoord,sizeof(core::vector2df));
            WMOMTexcoord.push_back(tempWMOMTexcoord);
        }
        DEBUG(logdev("Read %u/%u Texture Coordinates",WMOMTexcoord.size(),(size/sizeof(core::vector2df))));

     }
    else if(!strcmp((char*)fourcc,"MOCV")){//Vertex colors!! Scaaaary!
        if(WMOMVertexColor.size()>0)
            WMOMVertexColor.clear();

        WMOColor tempWMOMVertexColor;

        for(u32 i =0;i<(size/sizeof(WMOColor));i++)
        {
            MeshFile->read(&tempWMOMVertexColor,sizeof(WMOColor));
            WMOMVertexColor.push_back(video::SColor(tempWMOMVertexColor.a,tempWMOMVertexColor.r,tempWMOMVertexColor.g,tempWMOMVertexColor.b));
        }
        DEBUG(logdev("Read %u/%u Vertex colors",WMOMVertexColor.size(),(size/sizeof(WMOColor))));

     }
     //End Group file parsing


     else
        MeshFile->seek(size,true);//Skip Chunk
}

if(!isRootFile)//If we just read a group file, add a mesh buffer to the main Mesh
{
if(WMOVertices.size()>0)
    WMOVertices.clear();

for(u32 i=0;i<WMOMVertices.size();i++)
{
    //WMOMVertices, Normals and stuff are not usable like this. Thus we build Vertices in irrlicht format
    //rotation happens when reading from file, so swapping Y and Z here is no longer necessary
    WMOVertices.push_back(video::S3DVertex(WMOMVertices[i],WMOMNormals[i], video::SColor(255,100,100,100),WMOMTexcoord[i]));
}

u32 lastindex=0;
for(u32 i=0;i<submeshes.size();i++)//The mesh has to be split into submeshes because irrlicht only handles 1 texture per meshbuffer (not quite correct but i am to lazy to explain now)
    {
    if(WMOMTexData[lastindex].textureID!=255)
    {
        scene::SSkinMeshBuffer *MeshBuffer = Mesh->addMeshBuffer(0);

        //Put the Indices and Vertices of the Submesh into a mesh buffer
        for(u32 j=lastindex;j<submeshes[i];j++)
        {
            if((j*3+2)<WMOMIndices.size()&&WMOMTexData[j].textureID!=255)
                {
                MeshBuffer->Indices.push_back(WMOMIndices[j*3]);
                MeshBuffer->Indices.push_back(WMOMIndices[j*3+1]);
                MeshBuffer->Indices.push_back(WMOMIndices[j*3+2]);
                }
        }
        DEBUG(logdev("Inserted %u Indices",MeshBuffer->Indices.size()));

        for(u32 j=0;j<WMOVertices.size();j++)
        {
            MeshBuffer->Vertices_Standard.push_back(WMOVertices[j]);
        }

        DEBUG(logdev("Inserted %u Vertices",MeshBuffer->Vertices_Standard.size()));

//         std::string TexName=Texdir.c_str();
//         TexName+="/";
//             TexName+=WMOMTextureFiles[WMOMTexData[lastindex].textureID].c_str();
//         while(TexName.find('\\')<TexName.size())//Replace \ by /
//             {
//             TexName.replace(TexName.find('\\'),1,"/");
//             }
//         while(TexName.find(' ')<TexName.size())//Replace space by _
//             {
//             TexName.replace(TexName.find(' '),1,"_");
//             }
//         std::transform(TexName.begin(), TexName.end(), TexName.begin(), tolower);
        char buf[1000];
        MemoryDataHolder::MakeTextureFilename(buf,WMOMTextureFiles[WMOMTexData[lastindex].textureID].c_str());
        video::ITexture* tex = Device->getVideoDriver()->findTexture(buf);
        if(!tex)
        {
          io::IReadFile* TexFile = io::IrrCreateIReadFileBasic(Device, buf);
//             logdebug("Texture %s loading",M2MTextureFiles[M2MTextureLookup[M2MTextureUnit[j].textureIndex]].c_str());
          if (!TexFile)
          {
              logerror("CM2MeshFileLoader: Texture file not found: %s", buf);
              continue;
          }
//             logdebug("Texture %s loaded",M2MTextureFiles[M2MTextureLookup[M2MTextureUnit[j].textureIndex]].c_str());
        tex = Device->getVideoDriver()->getTexture(TexFile);
        TexFile->drop();
        }

        MeshBuffer->getMaterial().setTexture(0,tex);
        if(WMOMTexDefinition[WMOMTexData[lastindex].textureID].blendMode==1)
            MeshBuffer->getMaterial().MaterialType=video::EMT_TRANSPARENT_ALPHA_CHANNEL;
        MeshBuffer->recalculateBoundingBox();
        MeshBuffer->setHardwareMappingHint(EHM_STATIC);
    }
    lastindex=submeshes[i];

}

WMOMIndices.clear();
WMOMVertices.clear();
WMOMNormals.clear();
WMOMTexcoord.clear();
WMOMVertexColor.clear();

WMOVertices.clear();
}
return true;

}

}
}
