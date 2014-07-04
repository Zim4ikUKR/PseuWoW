/** Example 009 Mesh Viewer

This tutorial show how to create a more complex application with the engine.
We construct a simple mesh viewer using the user interface API and the
scene management of Irrlicht. The tutorial show how to create and use Buttons,
Windows, Toolbars, Menus, ComboBoxes, Tabcontrols, Editboxes, Images,
MessageBoxes, SkyBoxes, and how to parse XML files with the integrated XML
reader of the engine.

We start like in most other tutorials: Include all nesessary header files, add
a comment to let the engine be linked with the right .lib file in Visual
Studio, and declare some global variables. We also add two 'using namespace'
statements, so we do not need to write the whole names of all classes. In this
tutorial, we use a lot stuff from the gui namespace.
*/
#include <irrlicht/irrlicht.h>
#include <iostream>
#include "common.h"
#include "os.h"
#include "GUI/CM2MeshFileLoader.h"
#include "GUI/CWMOMeshFileLoader.h"
#include "GUI/MemoryInterface.h"
#include "MemoryDataHolder.h"
#include "GUI/CM2MeshSceneNode.h"


using namespace irr;
using namespace gui;

#ifdef _MSC_VER
#pragma comment(lib, "Irrlicht.lib")
#endif


/*
Some global variables used later on
*/
IrrlichtDevice *Device = 0;
core::stringc StartUpModelFile = "";
core::stringw MessageText;
core::stringw Caption;
scene::ISceneNode* Model = 0;
scene::ISceneNode* SkyBox = 0;
gui::IGUITreeView* TreeView = 0;
bool Octree=false;


scene::ICameraSceneNode* Camera[3] = {0, 0, 0};

// Values used to identify individual GUI elements
enum
{
	GUI_ID_DIALOG_ROOT_WINDOW  = 0x10000,

	GUI_ID_X_SCALE,
	GUI_ID_Y_SCALE,
	GUI_ID_Z_SCALE,

    GUI_ID_LIGHT_BOX,
    GUI_ID_LIGHT_X_SCALE,
    GUI_ID_LIGHT_Y_SCALE,
    GUI_ID_LIGHT_Z_SCALE,
    GUI_ID_LIGHT_VISIBLE,
    GUI_ID_LIGHT_SET,

	GUI_ID_OPEN_MODEL,
	GUI_ID_SET_MODEL_ARCHIVE,
	GUI_ID_LOAD_AS_OCTREE,

	GUI_ID_SKY_BOX_VISIBLE,
	GUI_ID_TOGGLE_DEBUG_INFO,

	GUI_ID_DEBUG_OFF,
	GUI_ID_DEBUG_BOUNDING_BOX,
	GUI_ID_DEBUG_NORMALS,
	GUI_ID_DEBUG_SKELETON,
	GUI_ID_DEBUG_WIRE_OVERLAY,
	GUI_ID_DEBUG_HALF_TRANSPARENT,
	GUI_ID_DEBUG_BUFFERS_BOUNDING_BOXES,
	GUI_ID_DEBUG_ALL,

	GUI_ID_MODEL_MATERIAL_SOLID,
	GUI_ID_MODEL_MATERIAL_TRANSPARENT,
	GUI_ID_MODEL_MATERIAL_REFLECTION,

	GUI_ID_CAMERA_MAYA,
	GUI_ID_CAMERA_FIRST_PERSON,
    GUI_ID_CAMERA_FIXED,

	GUI_ID_POSITION_TEXT,

	GUI_ID_ABOUT,
	GUI_ID_QUIT,

    GUI_ID_TREE_VIEW,

    GUI_ID_FRAME_START,
    GUI_ID_FRAME_END,
    GUI_ID_FRAME_SET,
    GUI_ID_FRAME_ANIM,
    GUI_ID_FRAME_SET_ANIM,
    GUI_ID_FRAME_SUBMESH,
    GUI_ID_FRAME_SET_SUBMESH,

	// And some magic numbers
	MAX_FRAMERATE = 1000,
	DEFAULT_FRAMERATE = 30,

    GUI_ID_LOAD_FILENAME,
    GUI_ID_LOAD_BUTTON,

    LIGHT_ID_0,
    LIGHT_ID_1,
    LIGHT_ID_2,
    LIGHT_ID_3

};

/*
Toggle between various cameras
*/
void setActiveCamera(scene::ICameraSceneNode* newActive)
{
	if (0 == Device)
		return;

	scene::ICameraSceneNode * active = Device->getSceneManager()->getActiveCamera();
	active->setInputReceiverEnabled(false);

	newActive->setInputReceiverEnabled(true);
	Device->getSceneManager()->setActiveCamera(newActive);
}

/*
The second function loadModel() loads a model and displays it using an
addAnimatedMeshSceneNode and the scene manager. Nothing difficult. It also
displays a short message box, if the model could not be loaded.
*/
void loadModel(const c8* fn)
{
	// modify the name if it a .pk3 file

	core::stringc filename(fn);

	core::stringc extension;
	core::getFileNameExtension(extension, filename);
	extension.make_lower();
    io::IReadFile* modelfile = io::IrrCreateIReadFileBasic(Device, filename.c_str());

	// if a texture is loaded apply it to the current model..
	if (extension == ".jpg" || extension == ".pcx" ||
		extension == ".png" || extension == ".ppm" ||
		extension == ".pgm" || extension == ".pbm" ||
		extension == ".psd" || extension == ".tga" ||
		extension == ".bmp" || extension == ".wal" || extension == ".blp")
	{
		video::ITexture * texture =
			Device->getVideoDriver()->getTexture( modelfile );
		if ( texture && Model )
		{
			// always reload texture
			Device->getVideoDriver()->removeTexture(texture);
			texture = Device->getVideoDriver()->getTexture( modelfile );

			Model->setMaterialTexture(0, texture);
		}
		return;
	}
	// if a archive is loaded add it to the FileSystems..
	else if (extension == ".pk3" || extension == ".zip")
	{
		Device->getFileSystem()->addZipFileArchive(filename.c_str());
		return;
	}
	else if (extension == ".pak")
	{
		Device->getFileSystem()->addPakFileArchive(filename.c_str());
		return;
	}

	// load a model into the engine

	if (Model)
		Model->remove();

	Model = 0;

	scene::IAnimatedMesh* m = Device->getSceneManager()->getMesh( modelfile );

	if (!m)
	{
		// model could not be loaded

		if (StartUpModelFile != filename)
			Device->getGUIEnvironment()->addMessageBox(
			Caption.c_str(), L"The model could not be loaded. " \
			L"Maybe it is not a supported file format.");
		return;
	}

	IGUITreeViewNode* treeroot = TreeView->getRoot();
    treeroot->clearChilds();
    treeroot=treeroot->addChildBack(core::stringw(filename).c_str());
    treeroot->setExpanded(true);
    for(u32 i=0;i<m->getMeshBufferCount();i++)
    {
      core::stringw nodename(L"Submesh "); 
      nodename += i;
      IGUITreeViewNode* treenode = treeroot->addChildBack(nodename.c_str());
      treenode->setExpanded(true);
      nodename = L"Texture: ";
      if(m->getMeshBuffer(i)->getMaterial().getTexture(0))
        nodename+= m->getMeshBuffer(i)->getMaterial().getTexture(0)->getName();
      else
        nodename += L"none";
      treenode->addChildBack(nodename.c_str());
      nodename = L"Material Type: ";
      switch(m->getMeshBuffer(i)->getMaterial().MaterialType)
      {
        case video::EMT_SOLID:
          nodename += "SOLID";
          break;
        case video::EMT_TRANSPARENT_ALPHA_CHANNEL_REF:
          nodename += "ALPHA_REF";
          break;
        case video::EMT_ONETEXTURE_BLEND:
          nodename += "BLEND";
          break;
      }
      treenode->addChildBack(nodename.c_str());
      nodename = L"FogEnable: ";
      nodename+= m->getMeshBuffer(i)->getMaterial().FogEnable?"true":"false";
      treenode->addChildBack(nodename.c_str());
      nodename = L"BackfaceCulling: ";
      nodename+= m->getMeshBuffer(i)->getMaterial().BackfaceCulling?"true":"false";
      treenode->addChildBack(nodename.c_str());
    }
//     IGUIElement* e = root->getElementFromId(GUI_ID_TREE_VIEW, true);
//     core::stringw str(L"");
//     str += L"Submeshes: ";
//     str.append(core::stringw(m->getMeshBufferCount()));
//     e->setText(str.c_str());
	// set default material properties

	//Log submesh info since treview can't side scroll
	FILE* s = fopen("viewer_submesh.txt","w");
	for(u32 i=0;i<((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes.size();i++)
    {  
		// ToDo:: rewrite the folowing section using one ostringstream to create the line of text and one write to the file per loop
	  // Create String handlers
      std::string info = "Submesh ";
	  std::ostringstream number;
	  std::ostringstream SubMeshMode;
	  std::ostringstream ShaderType; // is it a decal? etc...
	  std::ostringstream Block;
	  std::ostringstream RootBone;
	  std::ostringstream Radius;
	  std::ostringstream ZDepth;
	  std::ostringstream Dist;
	  std::ostringstream M2SubmeshID; // submesh's index in the m2/skin files 
	  // Slip titles into handlers
	  SubMeshMode<< "  Mode:";
	  ShaderType<< " ShaderType:";
	  Block<< " Block:";
	  RootBone<< " RootBone:";
	  Radius<< " Radius:";
	  ZDepth<< "Depth, Back:";
	  Dist<< "Distance: ";
	  M2SubmeshID<< " M2Submesh# ";
	  number<<  i;
	  // Put the data into the Handlers and Print it to the File
	  info += number.str();
	  fwrite(info.c_str(),1,info.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
      info = "Texture: ";
	  fwrite(info.c_str(),1,info.size(),s); // print to file
	  fseek(s,1,true); // Insert space in File
      // Get texture path and name
	  info = ((scene::CM2Mesh*)(m))->Textures[((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Textures[0].Path].c_str();
	  fwrite(info.c_str(),1,info.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File

      info = "Material Type: ";
      switch(((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Textures[0].BlendFlag)
      {
	  case 0:
          info += "SOLID";
          break;
	  case 1:
          info += "ALPHA_REF";
          break;
	  case 2:
          info += "ALPHA_BLEND";
          break;
	  case 3:
		  info += "ADDITIVE";
		  break;
	  case 4:
		  info += "ADDITIVE_ALPHA";
		  break;
	  case 5:
		  info += "MODULATE";
		  break;
	  case 6:
		  info += "MODULATE_2X ?";
		  break;
      }
	  fwrite(info.c_str(),1,info.size(),s); 
	  M2SubmeshID<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].LoaderIndex; // get the submesh index relative to the m2/skin files
	  std::string m2sid = M2SubmeshID.str();
	  fwrite(m2sid.c_str(),1,m2sid.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
      SubMeshMode<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Textures[0].Mode; //get submesh's mode
	  std::string Mode = SubMeshMode.str();
	  fwrite(Mode.c_str(),1,Mode.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
	  ShaderType<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Textures[0].shaderType; //((scene::CM2Mesh*)(m))->BufferMap[i].order;
	  Block<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Textures[0].Block;
	  RootBone<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].RootBone;         //BufferMap[i].unknown;
	  Radius<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Radius;
	  Dist<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Distance;
	  //ZDepth<< ((scene::CM2Mesh*)(m))->BufferMap[i].Back;
	  //ZDepth<< "Front,";
	  //ZDepth<< ((scene::CM2Mesh*)(m))->BufferMap[i].Front;
	  //ZDepth<< "Middle,";
	  //ZDepth<< ((scene::CM2Mesh*)(m))->BufferMap[i].Middle;
	  std::string TypeOfShader = ShaderType.str();
	  std::string block = Block.str();
	  std::string bone = RootBone.str();
	  std::string radius = Radius.str();
	  std::string distance = Dist.str();
	  //std::string zdepth = ZDepth.str();
	  fwrite(TypeOfShader.c_str(),1,TypeOfShader.size(),s);  // print to file
	  fseek(s,1,true); // Isert space in File
	  fwrite(block.c_str(),1,block.size(),s);  // print to file
	  fseek(s,1,true); // Isert space in File
	  fwrite(bone.c_str(),1,bone.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
	  fwrite(radius.c_str(),1,radius.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
	  fwrite(distance.c_str(),1,distance.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
	  //fwrite(zdepth.c_str(),1,zdepth.size(),s); // print to file
	  //fseek(s,1,true); // Isert space in File
      /*info = "FogEnable: ";
      info += m->getMeshBuffer(i)->getMaterial().FogEnable?"true":"false";
	  fwrite(info.c_str(),1,info.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
      info = "BackfaceCulling: ";
      info += m->getMeshBuffer(i)->getMaterial().BackfaceCulling?"true":"false";
	  fwrite(info.c_str(),1,info.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
	  */std::ostringstream flag;
	  flag<< "RenderFlag: ";
      flag<< ((scene::CM2Mesh*)(m))->Skins[((scene::CM2Mesh*)(m))->SkinID].Submeshes[i].Textures[0].RenderFlag;
	  std::string RFlag = flag.str();
	  fwrite(RFlag.c_str(),1,RFlag.size(),s); // print to file
	  fseek(s,1,true); // Isert space in File
	  fwrite("\r\n",1,4,s); //set eol (end of line)
    }
	fclose(s);
	//end log

	if (Octree)
		Model = Device->getSceneManager()->addOctTreeSceneNode(m->getMesh(0));
	else
	{
		scene::CM2MeshSceneNode* animModel = new CM2MeshSceneNode(m,Device->getSceneManager()->getRootSceneNode(),Device->getSceneManager(),-1);//fact->addM2SceneNode(m);
		//scene::IAnimatedMeshSceneNode* animModel = Device->getSceneManager()->addAnimatedMeshSceneNode(m);
		//core::array<scene::IBoneSceneNode*> ChildBoneSceneNodes;
		//((scene::CM2Mesh*)(m))->createJoints(ChildBoneSceneNodes, animModel, Device->getSceneManager());
		//((scene::CM2Mesh*)(m))->LinkChildMeshes(animModel, Device->getSceneManager(), ChildBoneSceneNodes); // link child mesh nodes
		animModel->setAnimationSpeed(1000);
        animModel->setM2Animation(0);
		Model = animModel;
		//ChildBoneSceneNodes.clear();
	}
	Model->setDebugDataVisible(scene::EDS_OFF);
	// we need to uncheck the menu entries. would be cool to fake a menu event, but
	// that's not so simple. so we do it brute force
	for(int id = GUI_ID_DEBUG_BOUNDING_BOX; id <= GUI_ID_DEBUG_WIRE_OVERLAY; ++id)
			((IGUICheckBox*)Device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(id,true))->setChecked(false);
	IGUIElement* toolboxWnd = Device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUI_ID_DIALOG_ROOT_WINDOW, true);
	if ( toolboxWnd )
	{
		toolboxWnd->getElementFromId(GUI_ID_X_SCALE, true)->setText(L"1.0");
		toolboxWnd->getElementFromId(GUI_ID_Y_SCALE, true)->setText(L"1.0");
		toolboxWnd->getElementFromId(GUI_ID_Z_SCALE, true)->setText(L"1.0");
	}

    FILE* f = fopen("viewer_last.txt","w");
    fwrite(filename.c_str(),1,filename.size(),f);
    fclose(f);

}


/*
Finally, the third function creates a toolbox window. In this simple mesh
viewer, this toolbox only contains a tab control with three edit boxes for
changing the scale of the displayed model.
*/
void createToolBox()
{
	// remove tool box if already there
	IGUIEnvironment* env = Device->getGUIEnvironment();
	IGUIElement* root = env->getRootGUIElement();
	IGUIElement* e = root->getElementFromId(GUI_ID_DIALOG_ROOT_WINDOW, true);
	if (e)
		e->remove();

	// create the toolbox window
	IGUIWindow* wnd = env->addWindow(core::rect<s32>(600,45,800,580),
		false, L"Toolset", 0, GUI_ID_DIALOG_ROOT_WINDOW);

	// create tab control and tabs
	IGUITabControl* tab = env->addTabControl(
		core::rect<s32>(2,20,800-602,580-7), wnd, true, true);

	IGUITab* t1 = tab->addTab(L"Config");

	// add some edit boxes and a button to tab one
    env->addStaticText(L"MPQ Filename:", core::rect<s32>(22,10,60,30), false, false, t1);
    env->addEditBox(L"Creature\\Wolf\\Wolf.m2", core::rect<s32>(10,35,190,55), true, t1, GUI_ID_LOAD_FILENAME);
    env->addButton(core::rect<s32>(10,65,155,85), t1, GUI_ID_LOAD_BUTTON, L"Load File from MPQ");


    env->addStaticText(L"Scale:",
			core::rect<s32>(10,90,150,110), false, false, t1);
	env->addStaticText(L"X:", core::rect<s32>(22,115,40,135), false, false, t1);
	env->addEditBox(L"1.0", core::rect<s32>(40,115,130,135), true, t1, GUI_ID_X_SCALE);
	env->addStaticText(L"Y:", core::rect<s32>(22,140,40,160), false, false, t1);
	env->addEditBox(L"1.0", core::rect<s32>(40,140,130,160), true, t1, GUI_ID_Y_SCALE);
	env->addStaticText(L"Z:", core::rect<s32>(22,165,40,182), false, false, t1);
	env->addEditBox(L"1.0", core::rect<s32>(40,165,130,185), true, t1, GUI_ID_Z_SCALE);

	env->addButton(core::rect<s32>(10,190,85,210), t1, 1101, L"Set");

	// add transparency control
	env->addStaticText(L"GUI Transparency Control:",
			core::rect<s32>(10,215,150,235), true, false, t1);
	IGUIScrollBar* scrollbar = env->addScrollBar(true,
			core::rect<s32>(10,240,150,260), t1, 104);
	scrollbar->setMax(255);
	scrollbar->setPos(255);

	// add framerate control
	env->addStaticText(L"Framerate:",
			core::rect<s32>(10,265,150,285), true, false, t1);
	scrollbar = env->addScrollBar(true,
			core::rect<s32>(10,290,150,310), t1, 105);
	scrollbar->setMax(MAX_FRAMERATE);
	scrollbar->setPos(DEFAULT_FRAMERATE);

    IGUITab* t2 = tab->addTab(L"Info");
    // add some edit boxes and a button to tab one
    env->addStaticText(L"Submeshes:",core::rect<s32>(10,20,150,45), false, false, t2);
    TreeView = env->addTreeView(core::rect<s32>(10,48,190,470), t2, GUI_ID_TREE_VIEW,true,true,true);


    IGUITab* t3 = tab->addTab(L"Lights");
    // add some edit boxes and a button to tab one
    IGUIComboBox* box =env->addComboBox(core::rect<s32>(10,20,150,45), t3, GUI_ID_LIGHT_BOX);
    box->addItem(L"Light 0");
    box->addItem(L"Light 1");
    box->addItem(L"Light 2");
    box->addItem(L"Light 3");

    env->addStaticText(L"X:", core::rect<s32>(22,48,40,66), false, false, t3);
    env->addEditBox(L"50.0", core::rect<s32>(40,46,130,66), true, t3, GUI_ID_LIGHT_X_SCALE);
    env->addStaticText(L"Y:", core::rect<s32>(22,82,40,96), false, false, t3);
    env->addEditBox(L"0.0", core::rect<s32>(40,76,130,96), true, t3, GUI_ID_LIGHT_Y_SCALE);
    env->addStaticText(L"Z:", core::rect<s32>(22,108,40,126), false, false, t3);
    env->addEditBox(L"0.0", core::rect<s32>(40,106,130,126), true, t3, GUI_ID_LIGHT_Z_SCALE);
    env->addCheckBox(true, core::rect<s32>(22,142,130,156),t3,GUI_ID_LIGHT_VISIBLE,L"Visible");
    env->addButton(core::rect<s32>(10,164,85,185), t3, GUI_ID_LIGHT_SET, L"Set");

    IGUITab* t4 = tab->addTab(L"Debug");
    env->addCheckBox(false, core::rect<s32>(22,48,130,68),t4,GUI_ID_DEBUG_BOUNDING_BOX,L"BBox");
    env->addCheckBox(false, core::rect<s32>(22,78,130,98),t4,GUI_ID_DEBUG_BUFFERS_BOUNDING_BOXES,L"Buffers BBoxes");
    env->addCheckBox(false, core::rect<s32>(22,108,130,128),t4,GUI_ID_DEBUG_HALF_TRANSPARENT,L"Half Transparent");
    env->addCheckBox(false, core::rect<s32>(22,138,130,158),t4,GUI_ID_DEBUG_NORMALS,L"Normals");
    env->addCheckBox(false, core::rect<s32>(22,168,130,188),t4,GUI_ID_DEBUG_SKELETON,L"Skeleton");
    env->addCheckBox(false, core::rect<s32>(22,198,130,218),t4,GUI_ID_DEBUG_WIRE_OVERLAY,L"Wire Overlay");
    env->addStaticText(L"Start:", core::rect<s32>(22,228,60,248), false, false, t4);
    env->addEditBox(L"0", core::rect<s32>(60,228,130,248), true, t4, GUI_ID_FRAME_START);
    env->addStaticText(L"End:", core::rect<s32>(22,258,60,278), false, false, t4);
    env->addEditBox(L"0", core::rect<s32>(60,258,130,278), true, t4, GUI_ID_FRAME_END);
    env->addButton(core::rect<s32>(10,288,85,308), t4, GUI_ID_FRAME_SET, L"Set");
    env->addStaticText(L"Animation:", core::rect<s32>(22,318,60,338), false, false, t4);
    env->addEditBox(L"0", core::rect<s32>(60,318,130,338), true, t4, GUI_ID_FRAME_ANIM);
    env->addButton(core::rect<s32>(10,348,85,368), t4, GUI_ID_FRAME_SET_ANIM, L"Set Anim");
    env->addStaticText(L"Submesh:", core::rect<s32>(22,378,60,398), false, false, t4);
    env->addEditBox(L"0", core::rect<s32>(60,378,130,398), true, t4, GUI_ID_FRAME_SUBMESH);
    env->addButton(core::rect<s32>(10,408,85,428), t4, GUI_ID_FRAME_SET_SUBMESH, L"Set Submesh");

    // bring irrlicht engine logo to front, because it
	// now may be below the newly created toolbox
	root->bringToFront(root->getElementFromId(666, true));
}


/*
To get all the events sent by the GUI Elements, we need to create an event
receiver. This one is really simple. If an event occurs, it checks the id of
the caller and the event type, and starts an action based on these values. For
example, if a menu item with id GUI_ID_OPEN_MODEL was selected, if opens a file-open-dialog.
*/
class MyEventReceiver : public IEventReceiver
{
public:
	virtual bool OnEvent(const SEvent& event)
	{
		// Escape swaps Camera Input
		if (event.EventType == EET_KEY_INPUT_EVENT &&
			event.KeyInput.PressedDown == false)
		{
			if (event.KeyInput.Key == irr::KEY_ESCAPE)
			{
				if (Device)
				{
					scene::ICameraSceneNode * camera =
						Device->getSceneManager()->getActiveCamera();
					if (camera)
					{
						camera->setInputReceiverEnabled( !camera->isInputReceiverEnabled() );
					}
					return true;
				}
			}
			else if (event.KeyInput.Key == irr::KEY_F1)
			{
				if (Device)
				{
					IGUIElement* elem = Device->getGUIEnvironment()->getRootGUIElement()->getElementFromId(GUI_ID_POSITION_TEXT);
					if (elem)
						elem->setVisible(!elem->isVisible());
				}
			}
		}

		if (event.EventType == EET_GUI_EVENT)
		{
			s32 id = event.GUIEvent.Caller->getID();
			IGUIEnvironment* env = Device->getGUIEnvironment();
            scene::ISceneManager* smgr = Device->getSceneManager();
			switch(event.GUIEvent.EventType)
			{
                case EGET_CHECKBOX_CHANGED:
                {
                    s32 pos = ((IGUICheckBox*)event.GUIEvent.Caller)->getID();
                    switch (pos)
                    {
                    case GUI_ID_DEBUG_BOUNDING_BOX: // View -> Debug Information
                        if (Model)
                            Model->setDebugDataVisible((scene::E_DEBUG_SCENE_TYPE)(Model->isDebugDataVisible()^scene::EDS_BBOX));
                        break;
                    case GUI_ID_DEBUG_NORMALS: // View -> Debug Information
                        if (Model)
                            Model->setDebugDataVisible((scene::E_DEBUG_SCENE_TYPE)(Model->isDebugDataVisible()^scene::EDS_NORMALS));
                        break;
                    case GUI_ID_DEBUG_SKELETON: // View -> Debug Information
                        if (Model)
                            Model->setDebugDataVisible((scene::E_DEBUG_SCENE_TYPE)(Model->isDebugDataVisible()^scene::EDS_SKELETON));
                        break;
                    case GUI_ID_DEBUG_WIRE_OVERLAY: // View -> Debug Information
                        if (Model)
                            Model->setDebugDataVisible((scene::E_DEBUG_SCENE_TYPE)(Model->isDebugDataVisible()^scene::EDS_MESH_WIRE_OVERLAY));
                        break;
                    case GUI_ID_DEBUG_HALF_TRANSPARENT: // View -> Debug Information
                        if (Model)
                            Model->setDebugDataVisible((scene::E_DEBUG_SCENE_TYPE)(Model->isDebugDataVisible()^scene::EDS_HALF_TRANSPARENCY));
                        break;
                    case GUI_ID_DEBUG_BUFFERS_BOUNDING_BOXES: // View -> Debug Information
                        if (Model)
                            Model->setDebugDataVisible((scene::E_DEBUG_SCENE_TYPE)(Model->isDebugDataVisible()^scene::EDS_BBOX_BUFFERS));
                        break;
                    }
                  break;
                }
			case EGET_MENU_ITEM_SELECTED:
				{
					// a menu item was clicked

					IGUIContextMenu* menu = (IGUIContextMenu*)event.GUIEvent.Caller;
					s32 id = menu->getItemCommandId(menu->getSelectedItem());

					switch(id)
					{
					case GUI_ID_OPEN_MODEL: // File -> Open Model
						env->addFileOpenDialog(L"Please select a model file to open");
						break;
					case GUI_ID_SET_MODEL_ARCHIVE: // File -> Set Model Archive
						env->addFileOpenDialog(L"Please select your game archive/directory");
						break;
					case GUI_ID_LOAD_AS_OCTREE: // File -> LoadAsOctree
						Octree = !Octree;
						menu->setItemChecked(menu->getSelectedItem(), Octree);
						break;
					case GUI_ID_QUIT: // File -> Quit
						Device->closeDevice();
						break;
					case GUI_ID_SKY_BOX_VISIBLE: // View -> Skybox
						menu->setItemChecked(menu->getSelectedItem(), !menu->isItemChecked(menu->getSelectedItem()));
						SkyBox->setVisible(!SkyBox->isVisible());
						break;
					case GUI_ID_MODEL_MATERIAL_SOLID: // View -> Material -> Solid
						if (Model)
							Model->setMaterialType(video::EMT_SOLID);
						break;
					case GUI_ID_MODEL_MATERIAL_TRANSPARENT: // View -> Material -> Transparent
						if (Model)
							Model->setMaterialType(video::EMT_TRANSPARENT_ALPHA_CHANNEL);
						break;
					case GUI_ID_MODEL_MATERIAL_REFLECTION: // View -> Material -> Reflection
						if (Model)
							Model->setMaterialType(video::EMT_SPHERE_MAP);
						break;

					case GUI_ID_CAMERA_MAYA:
						setActiveCamera(Camera[0]);
						break;
                    case GUI_ID_CAMERA_FIRST_PERSON:
                        setActiveCamera(Camera[1]);
                        break;
                    case GUI_ID_CAMERA_FIXED:
                        setActiveCamera(Camera[2]);
                        break;

					}
				break;
				}

			case EGET_SCROLL_BAR_CHANGED:

				// control skin transparency
				if (id == 104)
				{
					const s32 pos = ((IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
					for (s32 i=0; i<irr::gui::EGDC_COUNT ; ++i)
					{
						video::SColor col = env->getSkin()->getColor((EGUI_DEFAULT_COLOR)i);
						col.setAlpha(pos);
						env->getSkin()->setColor((EGUI_DEFAULT_COLOR)i, col);
					}
				}
				else if (id == 105)
				{
					const s32 pos = ((IGUIScrollBar*)event.GUIEvent.Caller)->getPos();
					if (scene::ESNT_ANIMATED_MESH == Model->getType())
						((scene::IAnimatedMeshSceneNode*)Model)->setAnimationSpeed((f32)pos);
				}
				break;

			case EGET_COMBO_BOX_CHANGED:
				// control anti-aliasing/filtering
				if (id == 108)
				{
					s32 pos = ((IGUIComboBox*)event.GUIEvent.Caller)->getSelected();
					switch (pos)
					{
						case 0:
						if (Model)
						{
							Model->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);
							Model->setMaterialFlag(video::EMF_TRILINEAR_FILTER, false);
							Model->setMaterialFlag(video::EMF_ANISOTROPIC_FILTER, false);
						}
						break;
						case 1:
						if (Model)
						{
							Model->setMaterialFlag(video::EMF_BILINEAR_FILTER, true);
							Model->setMaterialFlag(video::EMF_TRILINEAR_FILTER, false);
						}
						break;
						case 2:
						if (Model)
						{
							Model->setMaterialFlag(video::EMF_BILINEAR_FILTER, false);
							Model->setMaterialFlag(video::EMF_TRILINEAR_FILTER, true);
						}
						break;
						case 3:
						if (Model)
						{
							Model->setMaterialFlag(video::EMF_ANISOTROPIC_FILTER, true);
						}
						break;
						case 4:
						if (Model)
						{
							Model->setMaterialFlag(video::EMF_ANISOTROPIC_FILTER, false);
						}
						break;
					}
				}
                else if(id == GUI_ID_LIGHT_BOX)
                {
                    s32 pos = ((IGUIComboBox*)event.GUIEvent.Caller)->getSelected();
                    scene::ISceneNode* light;
                    switch (pos)
                    {
                    case 0:
                    light=smgr->getSceneNodeFromId(LIGHT_ID_0);
                    break;
                    case 1:
                    light=smgr->getSceneNodeFromId(LIGHT_ID_1);
                    break;
                    case 2:
                    light=smgr->getSceneNodeFromId(LIGHT_ID_2);
                    break;
                    case 3:
                    light=smgr->getSceneNodeFromId(LIGHT_ID_3);
                    break;
                    }
                core::vector3df lpos = light->getPosition();
                env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_X_SCALE,true)->setText(core::stringw(lpos.X).c_str());
                env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_Y_SCALE,true)->setText(core::stringw(lpos.Y).c_str());
                env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_Z_SCALE,true)->setText(core::stringw(lpos.Z).c_str());
                IGUICheckBox* box =(IGUICheckBox*)(env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_VISIBLE,true));
                box->setChecked(light->isVisible());
                }
				break;

			case EGET_BUTTON_CLICKED:

				switch(id)
				{
				case 1101:
					{
						// set scale
						gui::IGUIElement* root = env->getRootGUIElement();
						core::vector3df scale;
						core::stringc s;

						s = root->getElementFromId(GUI_ID_X_SCALE, true)->getText();
						scale.X = (f32)atof(s.c_str());
						s = root->getElementFromId(GUI_ID_Y_SCALE, true)->getText();
						scale.Y = (f32)atof(s.c_str());
						s = root->getElementFromId(GUI_ID_Z_SCALE, true)->getText();
						scale.Z = (f32)atof(s.c_str());

						if (Model)
							Model->setScale(scale);
					}
					break;
				case 1104:
					createToolBox();
					break;

                case GUI_ID_LIGHT_SET:
                {
                    scene::ISceneNode* light;

                    s32 pos=((IGUIComboBox*)env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_BOX,true))->getSelected();
                    switch (pos)
                    {
                        case 0:
                        light=smgr->getSceneNodeFromId(LIGHT_ID_0);
                        break;
                        case 1:
                        light=smgr->getSceneNodeFromId(LIGHT_ID_1);
                        break;
                        case 2:
                        light=smgr->getSceneNodeFromId(LIGHT_ID_2);
                        break;
                        case 3:
                        light=smgr->getSceneNodeFromId(LIGHT_ID_3);
                        break;
                    }
                    core::vector3df lpos = core::vector3df(atof(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_X_SCALE,true)->getText()).c_str()),
                                                           atof(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_Y_SCALE,true)->getText()).c_str()),
                                                           atof(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_Z_SCALE,true)->getText()).c_str()));
                    light->setPosition(lpos);
                    light->setVisible(((IGUICheckBox*)(env->getRootGUIElement()->getElementFromId(GUI_ID_LIGHT_VISIBLE,true)))->isChecked());
                }
                    break;
                case GUI_ID_FRAME_SET:
                {
                    if(Model)
                      ((scene::IAnimatedMeshSceneNode*)Model)->setFrameLoop(atoi(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_FRAME_START,true)->getText()).c_str()),
                                            atoi(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_FRAME_END,true)->getText()).c_str()));
                }
                    break;
                case GUI_ID_FRAME_SET_ANIM:
                {
                    if(Model)
                      ((scene::IAnimatedMeshSceneNode*)Model)->setM2Animation(atoi(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_FRAME_ANIM,true)->getText()).c_str()));
                }
                    break;
                case GUI_ID_FRAME_SET_SUBMESH:
                {
                    u32 submesh_id = atoi(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_FRAME_SUBMESH,true)->getText()).c_str());
                    if(Model)
                      ((scene::CM2Mesh*)((scene::IAnimatedMeshSceneNode*)Model)->getMesh())->setMBRender(submesh_id,!((scene::CM2Mesh*)((scene::IAnimatedMeshSceneNode*)Model)->getMesh())->getGeoSetRender(submesh_id));
                }
                    break;
                case GUI_ID_LOAD_BUTTON:
                {
                    loadModel(core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_LOAD_FILENAME,true)->getText()).c_str());
                    StartUpModelFile=core::stringc(env->getRootGUIElement()->getElementFromId(GUI_ID_LOAD_FILENAME,true)->getText()).c_str();
                }
                    break;
                }

				break;
			default:
				break;
			}
		}

		return false;
	}
};


/*
Most of the hard work is done. We only need to create the Irrlicht Engine
device and all the buttons, menus and toolbars. We start up the engine as
usual, using createDevice(). To make our application catch events, we set our
eventreceiver as parameter. The #ifdef WIN32 preprocessor commands are not
necessary, but I included them to make the tutorial use DirectX on Windows and
OpenGL on all other platforms like Linux. As you can see, there is also a
unusual call to IrrlichtDevice::setResizeAble(). This makes the render window
resizeable, which is quite useful for a mesh viewer.
*/
int main(int argc, char* argv[])
{
  //config hacks
  log_setloglevel(3);
  log_prepare("viewerlog.txt","w");
  MemoryDataHolder::SetUseMPQ("enUS");

  FILE* f;
  f = fopen("viewer_last.txt","r");
  if(f!=NULL)
  {
    log("Loading last used mesh");
    c8 buffer[255];
    memset(buffer,0,255);
    fseek(f,0,SEEK_SET);
    fread(&buffer,1,255,f);
    StartUpModelFile = buffer;
    fclose(f);
  }

  // ask user for driver
	video::E_DRIVER_TYPE driverType = video::EDT_DIRECT3D8;

	printf("Please select the driver you want for this example:\n"\
		" (a) Direct3D 9.0c\n (b) Direct3D 8.1\n (c) OpenGL 1.5\n"\
		" (d) Software Renderer\n (e) Burning's Software Renderer\n"\
		" (f) NullDevice\n (otherKey) exit\n\n");

	char key;
	std::cin >> key;

	switch(key)
	{
		case 'a': driverType = video::EDT_DIRECT3D9;break;
		case 'b': driverType = video::EDT_DIRECT3D8;break;
		case 'c': driverType = video::EDT_OPENGL;   break;
		case 'd': driverType = video::EDT_SOFTWARE; break;
		case 'e': driverType = video::EDT_BURNINGSVIDEO;break;
		case 'f': driverType = video::EDT_NULL;     break;
		default: return 1;
	}

	// create device and exit if creation failed

	MyEventReceiver receiver;
	Device = createDevice(driverType, core::dimension2d<u32>(800, 600),
		16, false, false, false, &receiver);

	if (Device == 0)
		return 1; // could not create selected driver.

	Device->setResizable(true);

	Device->setWindowCaption(L"Irrlicht Engine - Loading...");

	video::IVideoDriver* driver = Device->getVideoDriver();
	IGUIEnvironment* env = Device->getGUIEnvironment();
	scene::ISceneManager* smgr = Device->getSceneManager();
	
	scene::ISceneNodeFactory *fact = new CM2MeshSceneNodeFactory(smgr);
	smgr->registerSceneNodeFactory(fact);

	//smgr->getParameters()->setAttribute(scene::ALLOW_ZWRITE_ON_TRANSPARENT, true); // test if fix alpha blend order
	smgr->getParameters()->setAttribute(scene::COLLADA_CREATE_SCENE_INSTANCES, true);

    // register external loaders for not supported filetypes
    scene::CM2MeshFileLoader* m2loader = new scene::CM2MeshFileLoader(Device);
    smgr->addExternalMeshLoader(m2loader);
    scene::CWMOMeshFileLoader* wmoloader = new scene::CWMOMeshFileLoader(Device);
    smgr->addExternalMeshLoader(wmoloader);
	driver->setTextureCreationFlag(video::ETCF_ALWAYS_32_BIT, true);

// 	smgr->addLightSceneNode();
    smgr->addLightSceneNode(0, core::vector3df(50,0,0),
            video::SColorf(1.0f,1.0f,1.0f),100, LIGHT_ID_0);
    smgr->addLightSceneNode(0, core::vector3df(0,50,0),
            video::SColorf(1.0f,1.0f,1.0f),100, LIGHT_ID_1);
    smgr->addLightSceneNode(0, core::vector3df(0,0,50),
            video::SColorf(1.0f,1.0f,1.0f),100, LIGHT_ID_2);
    smgr->addLightSceneNode(0, core::vector3df(0,0,-50),
            video::SColorf(1.0f,1.0f,1.0f),100, LIGHT_ID_3);

    smgr->getSceneNodeFromId(LIGHT_ID_1)->setVisible(false);
    smgr->getSceneNodeFromId(LIGHT_ID_2)->setVisible(false);
    smgr->getSceneNodeFromId(LIGHT_ID_3)->setVisible(false);


	// set a nicer font

	IGUISkin* skin = env->getSkin();
	IGUIFont* font = env->getFont("./data/misc/fonthaettenschweiler.bmp");
	if (font)
		skin->setFont(font);

	// create menu
	gui::IGUIContextMenu* menu = env->addMenu();
	menu->addItem(L"File", -1, true, true);
	menu->addItem(L"View", -1, true, true);
	menu->addItem(L"Camera", -1, true, true);

	gui::IGUIContextMenu* submenu;
	submenu = menu->getSubMenu(0);

	submenu->addItem(L"Quit", GUI_ID_QUIT);

	submenu = menu->getSubMenu(1);
	submenu->addItem(L"sky box visible", GUI_ID_SKY_BOX_VISIBLE, true, false, true);
	submenu->addItem(L"toggle model debug information", GUI_ID_TOGGLE_DEBUG_INFO, true, true);
	submenu->addItem(L"model material", -1, true, true );

	submenu = menu->getSubMenu(1)->getSubMenu(2);
	submenu->addItem(L"Solid", GUI_ID_MODEL_MATERIAL_SOLID);
	submenu->addItem(L"Transparent", GUI_ID_MODEL_MATERIAL_TRANSPARENT);
	submenu->addItem(L"Reflection", GUI_ID_MODEL_MATERIAL_REFLECTION);

	submenu = menu->getSubMenu(2);
	submenu->addItem(L"Maya Style", GUI_ID_CAMERA_MAYA);
    submenu->addItem(L"First Person", GUI_ID_CAMERA_FIRST_PERSON);
    submenu->addItem(L"Fixed Camera", GUI_ID_CAMERA_FIXED);


	/*
	Below the menu we want a toolbar, onto which we can place colored
	buttons and important looking stuff like a senseless combobox.
	*/

	// create toolbar

	gui::IGUIToolBar* bar = env->addToolBar();

	video::ITexture* image = driver->getTexture("./data/misc/tools.png");
	bar->addButton(1104, 0, L"Open Toolset",image, 0, false, true);

// 	image = driver->getTexture("./data/misc/zip.png");
// 	bar->addButton(1105, 0, L"Set Model Archive",image, 0, false, true);

	// create a combobox with some senseless texts

	gui::IGUIComboBox* box = env->addComboBox(core::rect<s32>(250,4,350,23), bar, 108);
	box->addItem(L"No filtering");
	box->addItem(L"Bilinear");
	box->addItem(L"Trilinear");
	box->addItem(L"Anisotropic");
	box->addItem(L"Isotropic");

	/*
	To make the editor look a little bit better, we disable transparent gui
	elements, and add an Irrlicht Engine logo. In addition, a text showing
	the current frames per second value is created and the window caption is
	changed.
	*/

	// disable alpha

	for (s32 i=0; i<gui::EGDC_COUNT ; ++i)
	{
		video::SColor col = env->getSkin()->getColor((gui::EGUI_DEFAULT_COLOR)i);
		col.setAlpha(255);
		env->getSkin()->setColor((gui::EGUI_DEFAULT_COLOR)i, col);
	}

	// add a tabcontrol

	createToolBox();

	// create fps text

	IGUIStaticText* fpstext = env->addStaticText(L"",
			core::rect<s32>(400,4,570,23), true, false, bar);

	IGUIStaticText* postext = env->addStaticText(L"",
			core::rect<s32>(10,50,470,80),false, false, 0, GUI_ID_POSITION_TEXT);
	postext->setVisible(false);

	// set window caption

	Caption += " - [";
	Caption += driver->getName();
	Caption += "]";
	Device->setWindowCaption(Caption.c_str());

	/*
	That's nearly the whole application. We simply show the about message
	box at start up, and load the first model. To make everything look
	better, a skybox is created and a user controled camera, to make the
	application a little bit more interactive. Finally, everything is drawn
	in a standard drawing loop.
	*/

	// show about message box and load default model
// 	if (argc==1)
// 		showAboutText();
    if(StartUpModelFile.c_str()!="")
      loadModel(StartUpModelFile.c_str());

	// add skybox

	SkyBox = smgr->addSkyBoxSceneNode(
		driver->getTexture("./data/misc/sky.jpg"),
		driver->getTexture("./data/misc/sky.jpg"),
		driver->getTexture("./data/misc/sky.jpg"),
		driver->getTexture("./data/misc/sky.jpg"),
		driver->getTexture("./data/misc/sky.jpg"),
		driver->getTexture("./data/misc/sky.jpg"));

	// add a camera scene node
	Camera[0] = smgr->addCameraSceneNodeMaya();
	Camera[0]->setFarValue(20000.f);
	// Maya cameras reposition themselves relative to their target, so target the location
	// where the mesh scene node is placed.
	Camera[0]->setTarget(core::vector3df(0,0,0));

	Camera[1] = smgr->addCameraSceneNodeFPS(0,50.0f,0.1f);
	Camera[1]->setFarValue(20000.f);
	Camera[1]->setPosition(core::vector3df(0,0,-70));
	Camera[1]->setTarget(core::vector3df(0,0,0));
    //Fixed camera set to the position of the cam in WOTLK Login
    Camera[2] = smgr->addCameraSceneNode(0,core::vector3df(11.11f,2.44f,-0.03f),core::vector3df(-10.28f,2.44f,-0.04f));
    Camera[2]->setFarValue(2777.7f);
    Camera[2]->setNearValue(0.222f);


	setActiveCamera(Camera[2]);

	// load the irrlicht engine logo
	IGUIImage *img =
		env->addImage(driver->getTexture("./data/misc/irrlichtlogo.png"),
			core::position2d<s32>(10, driver->getScreenSize().Height - 128));

	// lock the logo's edges to the bottom left corner of the screen
	img->setAlignment(EGUIA_UPPERLEFT, EGUIA_UPPERLEFT,
			EGUIA_LOWERRIGHT, EGUIA_LOWERRIGHT);



	// draw everything

	while(Device->run() && driver)
	{
		if (Device->isWindowActive())
		{
			driver->beginScene(true, true, video::SColor(150,50,50,50));

            smgr->drawAll();

            video::SMaterial m;
            m.Lighting = false;
            driver->setMaterial(m);
            driver->setTransform(video::ETS_WORLD, core::matrix4());
            driver->draw3DLine(core::vector3df(-1,-1,-1),core::vector3df(10,-1,-1),video::SColor(255,255,0,0));
            driver->draw3DLine(core::vector3df(-1,-1,-1),core::vector3df(-1,10,-1),video::SColor(255,0,255,0));
            driver->draw3DLine(core::vector3df(-1,-1,-1),core::vector3df(-1,-1,10),video::SColor(255,0,0,255));
            driver->draw3DLine(core::vector3df(-1,-1,-1),core::vector3df(0,0,0),video::SColor(255,255,0,255));

            env->drawAll();

			driver->endScene();

			core::stringw str(L"FPS: ");
			str.append(core::stringw(driver->getFPS()));
			str += L" Tris: ";
			str.append(core::stringw(driver->getPrimitiveCountDrawn()));
            str += L" Frame: ";
            if(Model)
              str.append(core::stringw(((scene::IAnimatedMeshSceneNode*)Model)->getFrameNr()));
			fpstext->setText(str.c_str());

			scene::ICameraSceneNode* cam = Device->getSceneManager()->getActiveCamera();
			str = L"Pos: ";
			str.append(core::stringw(cam->getPosition().X));
			str += L" ";
			str.append(core::stringw(cam->getPosition().Y));
			str += L" ";
			str.append(core::stringw(cam->getPosition().Z));
			str += L" Tgt: ";
			str.append(core::stringw(cam->getTarget().X));
			str += L" ";
			str.append(core::stringw(cam->getTarget().Y));
			str += L" ";
			str.append(core::stringw(cam->getTarget().Z));
			postext->setText(str.c_str());

        }
		else
			Device->yield();
	}


	Device->drop();
	return 0;
}
