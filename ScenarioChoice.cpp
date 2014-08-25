#include "ScenarioChoice.hpp"
#include "StartupEventReceiver.hpp"

#include <iostream>

using namespace irr;

ScenarioChoice::ScenarioChoice(irr::IrrlichtDevice* device)
{
    this->device = device;
    gui = device->getGUIEnvironment();
}

std::string ScenarioChoice::chooseScenario()
{
    video::IVideoDriver* driver = device->getVideoDriver();

    //Get list of scenarios, stored in scenarioList
    std::vector<std::string> scenarioList;
    getScenarioList(scenarioList,"Scenarios/"); //Populate list //Fixme: Scenarios path duplicated here and in SimulationModel

    //Make gui elements
    core::stringw titleText = L"Bridge Command 5.0 Alpha 1"; //Fixme: Get version automatically
    core::dimension2d<u32> titleDimensions = gui->getSkin()->getFont()->getDimension(titleText.c_str());
    gui::IGUIListBox* scenarioListBox = gui->addListBox(core::rect<s32>(10,30,110,230),0,GUI_ID_SCENARIO_LISTBOX);
    gui::IGUIButton* okButton = gui->addButton(core::rect<s32>(10,240,110,260),0,GUI_ID_OK_BUTTON,L"OK"); //i18n?
    gui::IGUIStaticText* instruction = gui->addStaticText(L"Please choose a scenario:",core::rect<s32>(10,10,110, 30)); //i18n
    gui::IGUIStaticText* title = gui->addStaticText(titleText.c_str(),core::rect<s32>((800-titleDimensions.Width)/2, 10, (800+titleDimensions.Width)/2, 30)); //FIXME: Hardcoding for window width 800
    //Add scenarios to list box
    for (std::vector<std::string>::iterator it = scenarioList.begin(); it != scenarioList.end(); ++it) {
        scenarioListBox->addItem(core::stringw(it->c_str()).c_str()); //Fixme!
    }
    //Link to our event receiver
    StartupEventReceiver startupReceiver(scenarioListBox,GUI_ID_SCENARIO_LISTBOX,GUI_ID_OK_BUTTON);
    device->setEventReceiver(&startupReceiver);

    while(device->run() && startupReceiver.getScenarioSelected()==-1) {
        if (device->isWindowActive())
        {
            //Event receiver will set Scenario Selected, so we just loop here until that happens
            driver->beginScene(true, true, video::SColor(0,200,200,200));
            gui->drawAll();
            driver->endScene();
        }
    }

    //Get name of selected scenario
    if (startupReceiver.getScenarioSelected()<0 || startupReceiver.getScenarioSelected() >= scenarioList.size()) {
        exit(EXIT_FAILURE); //No scenario loaded
    }
    std::string scenarioName = scenarioList[startupReceiver.getScenarioSelected()];

    //Clean up
    scenarioListBox->remove(); scenarioListBox = 0;
    okButton->remove(); okButton = 0;
    title->remove(); title = 0;
    instruction->remove(); instruction=0;
    device->setEventReceiver(0); //Remove link to startup event receiver, as this will be destroyed.

    //Show loading message
    gui::IGUIStaticText* loadingMessage = gui->addStaticText(L"Loading...", core::rect<s32>(10,10,210,20)); //i18n
    device->run();
    driver->beginScene(true, true, video::SColor(0,200,200,200));
    gui->drawAll();
    driver->endScene();
    loadingMessage->remove(); loadingMessage = 0;

    return scenarioName;
}

void ScenarioChoice::getScenarioList(std::vector<std::string>&scenarioList, std::string scenarioPath) {

    io::IFileSystem* fileSystem = device->getFileSystem();
    if (fileSystem==0) {
        exit(EXIT_FAILURE); //Could not get file system TODO: Message for user
    }
    //store current dir
    io::path cwd = fileSystem->getWorkingDirectory();

    //change to scenario dir
    if (!fileSystem->changeWorkingDirectoryTo(scenarioPath.c_str())) {
        exit(EXIT_FAILURE); //Couldn't change to scenario dir
    }

    io::IFileList* fileList = fileSystem->createFileList();
    if (fileList==0) {
        exit(EXIT_FAILURE); //Could not get file list for scenarios TODO: Message for user
    }

    //List here
    for (u32 i=0;i<fileList->getFileCount();i++) {
        if (fileList->isDirectory(i)) {
            const io::path& fileName = fileList->getFileName(i);
            if (fileName.findFirst('.')!=0) { //Check it doesn't start with '.' (., .., or hidden)
                //std::cout << fileName.c_str() << std::endl;
                scenarioList.push_back(fileName.c_str());
            }
        }
    }

    //change back
    if (!fileSystem->changeWorkingDirectoryTo(cwd)) {
        exit(EXIT_FAILURE); //Couldn't change dir back
    }
    fileList->drop();
}