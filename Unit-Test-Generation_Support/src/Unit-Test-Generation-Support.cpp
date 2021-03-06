/*
TODO: Take care of local variables shading global variables. 
You have already see a strange case, that made you lose a lot of time in 
debugging. 
*/

#include <iostream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <sstream>
#include <vector>
#include <string>
#include <set>
#include <utility>
#include <algorithm> 
#include <experimental/filesystem>
#include <cstdint>
#include <windows.h>

#include "json.hpp"

#include "Variabile.h"
#include "Unit-Test-Generation-Support.h"
#include "Node.h"
#include "CoreTypes.h"
#include "RegexColl.h"
#include "SizeofTypes.h"
#include "Logger.h"
#include "main.h"
#include "forGUI.h"
#include "ValuesFile.h"
#include "TestRunner.h"


// vVariable contains all the global and local variables defined.
VariableShadow vShadowedVar;
// vStruct contains all the struct definition
std::vector<Variable> vStruct;
// vStruct contains all the union definition
std::vector<Variable> vUnion;
// vTypeDef contains all the user defined types
std::vector<Variable> vTypeDef;
// vEnum contains all the user defined enums
std::vector<Variable> vEnum;
// vFunction contains the declared function
std::vector<Variable> vFunction;
// vParams contains the temporary function parameters
std::vector<Variable> vParams;
std::ofstream Variable::outputFile;
int shadowLevel = 0;
struct SourceRef_s::SourcePoint_s globalSource;
extern std::string splash;
Variable* myP;
TestRunner testRunner;

unsigned long long cast(std::string str, unsigned long long v);

int inner_main(int argc, std::string argv[]) throw (const std::exception&)
{
    /*
    argv[1] = ast complete filename
    argv[2] = result complete filename
    argv[3] = base directory
    argv[4] = test folder 
    */
    if (argc != 5) {
#if __GNUC__
        throw std::exception();
#else
        throw std::exception("\n\n*** There must be 2 arguments: ast file name and output file name. ***\n\n");
#endif
    }
    std::cout << "Unit Test Generation Support!\n";
    std::cout << splash;

    if (std::regex_match("subject", std::regex("(sub)(.*)")))
        std::cout << "string literal matched\n";

//    Variable::outputFile.open(argv[2], std::ios_base::trunc);
//    if (!Variable::outputFile) {
//#if __GNUC__
//        throw std::exception();
//#else
//        throw std::exception("\n\n*** Cannot open output file. ***\n\n");
//#endif
//    }

    std::ifstream infile(argv[1]);
    if (!infile) {
#if __GNUC__
        throw std::exception();
#else
        throw std::exception("\n\n*** Ast file not found. ***\n\n");
#endif
    }
    std::string str;

    int depth = -2;
    int row = 1;
    Node myRoot;
    Node* cursor;
    cursor = &myRoot;
    //
    std::set<std::string> astTypesSet;
    std::smatch smCatchGlobals;
    std::smatch smCatchGlobalName;

    std::cout << "\n\nGlobal variables list and internal ast building\n\n";
    while (std::getline(infile, str)) {
        if (std::regex_search(str, smCatchGlobals, eCatchGlobals) && smCatchGlobals.size() >= 2) {
            if ((smCatchGlobals[1].length() == 2  // Will found only functions and global variables and skip local ones
                &&
                (smCatchGlobals[2].compare("VarDecl") == 0
                    ||
                    smCatchGlobals[2].compare("FunctionDecl") == 0))
                ||
                (smCatchGlobals[1].length() == 8
                    &&  // These are local variables
                    smCatchGlobals[2].compare("VarDecl") == 0)
                )
            {
                if (std::regex_search(str, smCatchGlobalName, eCatchGlobalName)) {
                    if (smCatchGlobals[1].length() == 2)
                        std::cout << "Found global ";
                    else
                        std::cout << "  Found local ";
                    std::cout << "name:type " << std::setw(30) << smCatchGlobalName[1] << ":" << smCatchGlobalName[2] << "\n";
                }
                else
                {
                    std::cout << "Not found global.  ";
                    std::cout << str << "\n";
                }
            }
            astTypesSet.insert(smCatchGlobals[2]);
            Node* tempNode = new Node();
            tempNode->text = str;
            tempNode->astFileRow = row++;
            tempNode->astType = smCatchGlobals[2];
            // Get the source reference that is the first part of the string until 
            // the location
            setSourceLocations(tempNode, str);
            // Get the complete source location
            // Did I find a sibling (on the same indentation level) ?
            if (smCatchGlobals[1].length() <= depth) {
                while (smCatchGlobals[1].length() < depth) {
                    cursor = cursor->parent;
                    depth = cursor->depth;
                }
                tempNode->parent = cursor->parent;
                tempNode->prevSib = cursor;
                tempNode->depth = depth;
                cursor->nextSib = tempNode;
                cursor = tempNode;
            }
            // Did I find a new child (on a deeper indentation level) ?
            if (smCatchGlobals[1].length() > depth) {
                depth = smCatchGlobals[1].length();
                tempNode->parent = cursor;
                tempNode->depth = depth;
                cursor->child = tempNode;
                cursor = tempNode;
            }
        }
    }
    /*
        Print AST types encountered
    */
    std::cout << "\n\nAst types encountered in the clang ast.\n\n";
    for (auto it = astTypesSet.begin(); it != astTypesSet.end(); it++) {
        std::cout << *it << "\n";
    }
    /*
        Visit the AST
    */
    Variable t;

    /* Assigna predefined size to all globals vectors.
    That will prevent the addresses to be changed because of reallocation.
    */
    createBuiltInTypes();

    // Write down ast node values
    vf = ValuesFile(forGui.valueFileName, myRoot.child);

    std::string baseDir = argv[3];
    std::string testFolder = argv[4];
    testRunner.inputValuesFile = baseDir + "test/" + testFolder + "/valuesInput.txt";
    testRunner.expectedValuesFile = baseDir + "test/" + testFolder + "/valuesExpected.txt";
    testRunner.neutralValuesFile = baseDir + "test/" + testFolder + "/valuesNeutral.txt";
    testRunner.startValuesFile = baseDir + "test/" + testFolder + "/valuesStart.txt";
    testRunner.callsFile = baseDir + "test/" + testFolder + "/valuesCalls.txt";

    testRunner.testState = TestRunner::TestState_enum::Init;

    if (myRoot.child) {
        bool runWhile = true;
        while (runWhile) {
            /* Pre visit processing*/
            switch (testRunner.testState) {
            case (TestRunner::TestState_enum::Init): {
                break;
            }
            case TestRunner::TestState_enum::BuildVariable: {
                testRunner.buildGlobals = true;
                /* This will prevent from entering and executing
                a test. */
                //testRunner.targetFunction = "fun1";
                testRunner.targetFunction = "ACM_ComputeDeltaAngle";
                break;
            }
            case TestRunner::TestState_enum::BeginOfFunction: {
                testRunner.cleanSetByUser = false;
                // The first time it enters here freeRunning is false
                //Logger(std::string("requested user") + " " + __FILE__ + " " + std::to_string(__LINE__));
                forGui.line = myRoot.child->sourceRef.Name.line - 1;
                forGui.col = myRoot.child->sourceRef.Name.col;
                forGui.astLine = 0;
                forGui.varName = "BeginOfFunction";
                forGui.len = 0;
                vf.writeFile();

                SetEvent(hEventReqGuiLine);
                WaitForSingleObject(hEventReqValue, INFINITE);
                if (forGui.ValueFromGui == 0) {
                    //testRunner.targetFunction = "fun1";
                    testRunner.targetFunction = "ACM_ComputeDeltaAngle";
                    testRunner.buildGlobals = false;
                    testRunner.freeRunning = true;
                    /* Reset Globals to allow const variables to set the setByUser and usedInTest flags*/
                    testRunner.cleanSetByUser = false;
                    /* Reset the function stores values (variables) to zero */
                    for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                        it->funcRetIndex = 0;
                    }
                    ResetGlobals();
                    /* Remove the calls file */
                    std::experimental::filesystem::remove(testRunner.callsFile);
                    testRunner.testState = TestRunner::TestState_enum::FreeRun;
                }
                else if (forGui.ValueFromGui == 1) {
                    testRunner.testState = TestRunner::TestState_enum::Finish;
                }
                else if (forGui.ValueFromGui == 2) {
                    forGui.strComm = "resetHL";
                    /*This request of user interaction is required to 
                    effectively clean the highlighted part of code in the GUI */
                    SetEvent(hEventReqGuiLine);
                    WaitForSingleObject(hEventReqValue, INFINITE);
                    testRunner.cleanSetByUser = true;
                    ResetGlobals();
                    //testRunner.targetFunction = "fun1";
                    testRunner.targetFunction = "ACM_ComputeDeltaAngle";
                    testRunner.buildGlobals = false;
                    testRunner.freeRunning = true;
                    testRunner.testState = TestRunner::TestState_enum::FreeRun;
                }
                break;
            }
            case TestRunner::TestState_enum::FreeRun: {
                testRunner.buildGlobals = false;
                testRunner.freeRunning = true;
                testRunner.cleanSetByUser = false;
                /* Reset the function stores values (variables) to zero */
                for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                    it->funcRetIndex = 0;
                }
                ResetGlobals();
                break;
            }
            case TestRunner::TestState_enum::LockedRun: {
                testRunner.buildGlobals = false;
                testRunner.freeRunning = false;
                testRunner.cleanSetByUser = false;
                /* Reset the function stores values (variables) to zero */
                for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                    it->funcRetIndex = 0;
                }
                ResetGlobals();
                break;
            }
            case TestRunner::TestState_enum::Finish: { 
                // Don't exaust the microprocessor
                Sleep(100); 
            }
            }

            /* Visit and execute the test */
            switch (testRunner.testState) {
            case TestRunner::TestState_enum::BuildVariable:
            case TestRunner::TestState_enum::LockedRun:
            case TestRunner::TestState_enum::FreeRun:
            {
                t = visit(myRoot.child);
                break;
            }
            }

            /* After visit processing*/
            switch (testRunner.testState) {
            case TestRunner::TestState_enum::Init: {
                testRunner.testState = TestRunner::TestState_enum::BuildVariable;
                break;
            }
            case TestRunner::TestState_enum::BuildVariable: {
                /* Copy variables values to input file */
                Variable::outputFile = std::ofstream(testRunner.startValuesFile);
                for (auto it = vShadowedVar.shadows.rbegin(); it != vShadowedVar.shadows.rend(); it++) {
                    for (auto itit = it->begin(); itit != it->end(); itit++) {
                        (*itit)->print(false);
                    }
                }
                testRunner.buildGlobals = false;
                testRunner.testState = TestRunner::TestState_enum::BeginOfFunction;
                break;
            }
            case TestRunner::TestState_enum::LockedRun: {
                vf.writeFile();

                /* If, at the end of a locked run, the freeRunning flag is still false, 
                it means that no interaction was required by the user, so the test has no 
                more variables to set, so this test run is finished or at least it makes no 
                sense to run it again. */
                if (testRunner.freeRunning == false) {
                    testRunner.testState = TestRunner::TestState_enum::BeginOfFunction;
                }
                else {
                    testRunner.testState = TestRunner::TestState_enum::FreeRun;
                }
                break;
            }
            case TestRunner::TestState_enum::FreeRun: {
                vf.writeFile();
                testRunner.testState = TestRunner::TestState_enum::LockedRun;
                break;
            }
            case TestRunner::TestState_enum::Finish: {
                /* Update json Calls file with target function parameters types and return type */
                std::ifstream callsFile(testRunner.callsFile);
                nlohmann::json j;
                if (callsFile.good() == true) {
                    callsFile >> j;
                    callsFile.close();
                }
                for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                    if (it->name.compare(testRunner.targetFunction) == 0) {
                        j["targetName"] = testRunner.targetFunction;
                        for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
                            if (itit->name.compare("returnValue") != 0) {
                                j["parameters"].push_back(nlohmann::json());
                                j["parameters"].back()["name"] = itit->name;
                                j["parameters"].back()["type"] = itit->type;
                                j["parameters"].back()["val"] = itit->value;
                            }
                            j["returnType"] = itit->type;
                            j["returnVal"] = itit->value;
                        }
                    }
                }
                std::ofstream callsFile_out(testRunner.callsFile, std::ofstream::out | std::ofstream::trunc);
                callsFile_out << j.dump(4);
                callsFile_out.close();
                /* Copy variables values to input file */
                Variable::outputFile = std::ofstream(testRunner.inputValuesFile);
                for (auto it = vShadowedVar.shadows.rbegin(); it != vShadowedVar.shadows.rend(); it++) {
                    for (auto itit = it->begin(); itit != it->end(); itit++) {
                        (*itit)->print("", ";", true);
                    }
                }
                Variable::outputFile.close();
                /* Append function under test parameters values to input files */
                Variable::outputFile = std::ofstream(testRunner.inputValuesFile, std::ofstream::out | std::ofstream::app);
                for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                    if (it->name.compare(testRunner.targetFunction) == 0) {
                        for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
                            itit->print("", ";", true);
                        }
                    }
                }
                /* Copy variables values to expected file */
                Variable::outputFile = std::ofstream(testRunner.expectedValuesFile);
                for (auto it = vShadowedVar.shadows.rbegin(); it != vShadowedVar.shadows.rend(); it++) {
                    for (auto itit = it->begin(); itit != it->end(); itit++) {
                        (*itit)->print("expected_", ";", false);
                    }
                }
                /* Print the expected return value */
                for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                    if (it->name.compare(testRunner.targetFunction) == 0) {
                        for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
                            if (itit->name.compare("returnValue") == 0) {
                                itit->print("expected_", ";", false);
                            }
                        }
                    }
                }
                Variable::outputFile.close();
                runWhile = false;
                break;
            }
            }
        } 
    }
    infile.close();
    Variable::outputFile.close();
    return 0;
}
void ResetGlobals(void) {
    for (auto it = vShadowedVar.shadows.begin(); it != vShadowedVar.shadows.end(); it++) {
        for (auto itit = (*it).begin(); itit != (*it).end(); itit++) {
            recurseVariable(*itit, NULL,
                [](Variable* va, Variable* vb) {
                    if (va->declaredConst == true || 
                        va->uData.myParent->declaredConst == true) {
                        va->usedInTest = true;
                        va->setByUser = true;
                    }
                    else {
                        va->usedInTest = false;
                    } 
                    if ( (va->typeEnum == Variable::typeEnum_t::isEnum ||  
                        va->declaredConst ||
                        va->uData.myParent->declaredConst) == false)  {
                        va->value = va->valueDefault;
                        va->valueDouble = va->valueDoubleDefault;
                    }
                    if (testRunner.cleanSetByUser == true) {
                        va->setByUser = false;
                    }
                }
            );
        }
    }
    for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
        /* Reset the function stores values (variables) to zero */
        it->funcRetIndex = 0;
        if (testRunner.cleanSetByUser == true) {
            it->functionReturns = std::vector<Variable>();
        }
        /* Now clean the parameters. */
        for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
            recurseVariable(&*itit, NULL,
                [](Variable* va, Variable* vb) {
                    va->usedInTest = false;
                    /* For enums it is wrong to erase the value because enums are 
                    constants which are set right from the beginning of the test*/
                    if (va->typeEnum != Variable::typeEnum_t::isEnum) {
                        va->value = va->valueDefault;
                        va->valueDouble = va->valueDoubleDefault;
                    }
                    if (testRunner.cleanSetByUser == true) {
                        va->setByUser = false;
                    }
                }
            );
        }
    }

}
void createBuiltInTypes(void)
{
    Variable temp;
    for (auto it = szTypes.begin(); it != szTypes.end(); it++) {
        temp.name = it->first;
        temp.type = it->first;
        temp.uData.uSize = BbSize(it->second, 0);
        temp.typeEnum = Variable::typeEnum_t::isValue;
        vTypeDef.push_back(temp);
    }
}
Variable visit(Node *node)
{
    //Logger(std::to_string(node->astFileRow));
    //std::cout << node->astFileRow << "\n";
    if (node->astFileRow == 35) {
        myP++;
    }
    if (node->astType.compare("IntegerLiteral") == 0) {
        return fIntegerLiteral(node);
    }
    if (node->astType.compare("FloatingLiteral") == 0) {
        return fFloatingLiteral(node);
    }
    if (node->astType.compare("VarDecl") == 0) {
        return fVarDecl(node);
    }
    if (node->astType.compare("TypedefDecl") == 0) {
        return fTypedefDecl(node);
    }
    if (node->astType.compare("DeclRefExpr") == 0) {
        return fDeclRefExpr(node);
    }
    if (node->astType.compare("ImplicitCastExpr") == 0) {
        return fImplicitCastExpr(node);
    }
    if (node->astType.compare("UnaryOperator") == 0) {
        return fUnaryOperator(node);
    }
    if (node->astType.compare("BinaryOperator") == 0) {
        return fBinaryOperator(node);
    }
    if (node->astType.compare("RecordDecl") == 0) {
        return fRecordDecl(node);
    }
    if (node->astType.compare("FieldDecl") == 0) {
        return fFieldDecl(node);
    }
    if (node->astType.compare("IndirectFieldDecl") == 0) {
        return fIndirectFieldDecl(node);
    }
    if (node->astType.compare("MemberExpr") == 0) {
        return fMemberExpr(node);
    }
    if (node->astType.compare("IfStmt") == 0) {
        return fIfStmt(node);
    }
    if (node->astType.compare("ForStmt") == 0) {
        return fForStmt(node);
    }
    if (node->astType.compare("CompoundStmt") == 0) {
        return fCompoundStmt(node);
    }
    if (node->astType.compare("TranslationUnitDecl") == 0) {
        return fTranslationUnitDecl(node);
    }
    if (node->astType.compare("ArraySubscriptExpr") == 0) {
        return fArraySubscriptExpr(node);
    }
    if (node->astType.compare("FunctionDecl") == 0) {
        return fFunctionDecl(node);
    }
    if (node->astType.compare("DeclStmt") == 0) {
        return fDeclStmt(node);
    }
    if (node->astType.compare("FullComment") == 0) {
        return fFullComment(node);
    }
    if (node->astType.compare("ConstantExpr") == 0) {
        return fConstantExpr(node);
    }
    if (node->astType.compare("EnumDecl") == 0) {
        return fEnumDecl(node);
    }
    if (node->astType.compare("EnumConstantDecl") == 0) {
        return fEnumConstantDecl(node);
    }
    if (node->astType.compare("ParmVarDecl") == 0) {
        return fParmVarDecl(node);
    }
    if (node->astType.compare("CallExpr") == 0) {
        return fCallExpr(node);
    }
    if (node->astType.compare("ParenExpr") == 0) {
        return fParenExpr(node);
    }
    if (node->astType.compare("ReturnStmt") == 0) {
        return fReturnStmt(node);
    }
    if (node->astType.compare("CStyleCastExpr") == 0) {
        return fCStyleCastExprt(node);
    }
    if (node->astType.compare("CompoundAssignOperator") == 0) {
        return fCompoundAssignOperator(node);
    }
    if (node->astType.compare("NullStmt") == 0) {
        return Variable();
    }
    if (node->astType.compare("WhileStmt") == 0) {
        return fWhileStmt(node);
    }
    if (node->astType.compare("InitListExpr") == 0) {
        return fInitListExpr(node);
    }
    if (node->astType.compare("ConditionalOperator") == 0) {
        return fConditionalOperator(node);
    }
    if (node->astType.compare("<<<NULL") == 0) {
        return fNULL(node);
    }
    throw std::string("Unknown ast node at " + std::to_string(node->astFileRow) + " " + node->text );
    if (node->child) {
        Variable t;
        t = visit(node->child);
    }

    if (node->nextSib) {
        Variable t;
        t = visit(node->nextSib);
    }
    return Variable();
}
Variable& valueCast(std::string str, Variable& v)
{
    if (str.compare("char") == 0) {
        v.value = (int8_t)v.value;
        v.uData.uSize = BbSize(1, 0);
        return v;
    }
    else if (str.compare("signed char") == 0) {
        v.value = (int8_t)v.value;
        v.uData.uSize = BbSize(1, 0);
        return v;
    }
    else if (str.compare("unsigned char") == 0) {
        v.value = (uint8_t)v.value;
        v.uData.uSize = BbSize(1, 0);
        return v;
    }
    else if (str.compare("short") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("short int") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("signed short") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("signed short int") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("unsigned short") == 0) {
        v.value = (uint16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("unsigned short int") == 0) {
        v.value = (uint16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("int") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("signed") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("signed int") == 0) {
        v.value = (int16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("unsigned") == 0) {
        v.value = (uint16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("unsigned int") == 0) {
        v.value = (uint16_t)v.value;
        v.uData.uSize = BbSize(2, 0);
        return v;
    }
    else if (str.compare("long") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("signed long") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("signed long int") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("unsigned long") == 0) {
        v.value = (uint32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("unsigned long int") == 0) {
        v.value = (uint32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("long long") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("long long int") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("signed long long") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("signed long long int") == 0) {
        v.value = (int32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("unsigned long long") == 0) {
        v.value = (uint32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("unsigned long long int") == 0) {
        v.value = (uint32_t)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("float") == 0) {
        v.value = (float)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("double") == 0) {
        v.value = (float)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    else if (str.compare("long double") == 0) {
        v.value = (float)v.value;
        v.uData.uSize = BbSize(4, 0);
        return v;
    }
    /* At this point we have encountered a typedef cast
    like a = 1; where a is a u8 a and u8 is a typedef.
    So we search the typedef, and we get the type.
    the "final" type should be a builtin or a pointer.
    Otherwise there shoukld be an explicit cast and that is handled otherwise.*/

    struct coreType_str coreTypes;
    coreTypes.coreType = str;
    findCoreType(coreTypes);
    /* Shouldn't be a pointer !!!
    So take only the final type */
    str = coreTypes.finalType;
    Variable temp;
    for (auto it = vTypeDef.begin(); it != vTypeDef.end(); it++) {
        if (it->name.compare(str) == 0) {
            temp = *it;
            break;
        }
    }
    // Try to cast again
    return valueCast(temp.type, v);

#if __GNUC__
    throw std::exception();
#else
    throw std::exception("cast: Exception encountered\n");
#endif
}
void signExtend(Variable* v) {
    unsigned long long val;
    val = v->value;
    if (szTypes[v->type] == 1) {
        val & 0x80 ? 0xFFFFFFFF & val : val;
    }
    else if (szTypes[v->type] == 2) {
        val & 0x8000 ? 0xFFFFFFFF & val : val;
    }
}
void expThrow(std::string mex) {
#if __GNUC__
    throw std::exception();
#else
    throw std::exception(mex.c_str());
#endif
}
Variable buildVariable(struct coreType_str& CoreTypes, Node* node) {
    bool found;
    std::smatch smTemp;
    std::smatch smStructTypeAnonymous;
    std::smatch smUnionTypeAnonymous;
    Variable temp;

    found = false;
    if (std::regex_search(CoreTypes.finalType, smTemp, std::regex("^struct\\s(.*)"))) {
        // Must take care of anonymous structs
        std::string typeName = smTemp[1];
        if (std::regex_search(typeName, smStructTypeAnonymous, eStructTypeAnonymous))
        {
            typeName = smStructTypeAnonymous[1];
        }
        for (auto t : vStruct) {
            if (t.name.compare(typeName) == 0) {
                temp = t;
                found = true;
            }
        }
    }
    else if (std::regex_search(CoreTypes.finalType, smTemp, std::regex("^union\\s(.*)"))) {
        // Must take care of anonymous unions ?
        std::string typeName = smTemp[1];
        if (std::regex_search(typeName, smUnionTypeAnonymous, eUnionTypeAnonymous))
        {
            typeName = smStructTypeAnonymous[1];
        }
        for (auto t : vUnion) {
            if (t.name.compare(typeName) == 0) {
                temp = t;
                found = true;
            }
        }
    }
    else if (std::regex_search(CoreTypes.finalType, smTemp, std::regex("^enum\\s(.*)"))) {
        // Must take care of anonymous enums ?
        std::string typeName = smTemp[1];
        for (auto t : vEnum) {
            if (t.name.compare(typeName) == 0) {
                temp = t;
                found = true;
            }
        }
    }
    else if (std::regex_search(CoreTypes.finalType, smTemp, eBuiltinTypes)) {
        temp.type = CoreTypes.finalType;
        temp.typeEnum = Variable::typeEnum_t::isValue;
        temp.uData.uSize = szTypes[temp.type];
        found = true;
    }
    else {
        // Search into typedefs
        for (auto t : vTypeDef) {
            if (t.name.compare(CoreTypes.finalType) == 0) {
                temp = t;
                found = true;
            }
        }
    }
    if (found == false) {
        /* Try to find the typedef with the hexID in the (anon structs) */
        /* To understand the following just look at how a typedef
        ast node is made. It must refer to other typedefs.
        */
        Node* node2 = node->child;
        while (node2) {
            std::smatch smTemp;
            if (std::regex_search(node2->text, smTemp, std::regex(
                "[^\\w<]*"                       /*| |-              */
                "(?:Record|Enum)"                /*Record or Enum    */
                "\\s"                            /*                  */
                "(0x[\\da-f]{6,11})"             /*0x247143d8308     */
            ))) {
                for (auto t : vStruct) {
                    if (t.hexID.compare(smTemp[1]) == 0) {
                        temp = t;
                        found = true;
                    }
                }
                if (found == false) {
                    for (auto t : vUnion) {
                        if (t.hexID.compare(smTemp[1]) == 0) {
                            temp = t;
                            found = true;
                        }
                    }
                }
                if (found == false) {
                    for (auto t : vEnum) {
                        if (t.hexID.compare(smTemp[1]) == 0) {
                            temp = t;
                            found = true;
                        }
                    }
                }
            }
            while (node2->nextSib) {

                node2 = node2->nextSib;
            }
            node2 = node2->child;
        }
    }
    if (found == false) {
#if __GNUC__
        throw std::exception();
#else
        throw std::exception("VarDecl: cannot find final type.");
#endif
    }


    int i;
    for (i = 0; i < CoreTypes.typeChainA.size(); i++) {
        if (CoreTypes.typeChainA.at(i).compare("*") == 0) {
            // It's omogeneous with the scan that end at i = CoreTypes.typeChainA.size()
            i++;
            break; // On the first pointer
        }
    }
    i--;
    for (; i >= 0; i--) {
        Variable vIt;
        if (std::regex_search(CoreTypes.typeChainA.at(i), smTemp, std::regex("\\[(\\d+)\\]"))) {
            /* An array */
            for (int ix = 0; ix < std::stoi(smTemp[1]); ix++) {
                vIt.typeEnum = Variable::typeEnum_t::isArray;
                vIt.array.push_back(temp);
            }
            vIt.uData.uSize = temp.uData.uSize * std::stoi(smTemp[1]);
            temp = vIt;
        }
        else if (std::regex_search(CoreTypes.typeChainA.at(i), smTemp, std::regex("\\*"))) {
            /* A pointer
               so the final type will be this pointer and not the previously found
               finalType. Reinitialize temp */
            temp = Variable();
            temp.typeEnum = Variable::typeEnum_t::isRef;
            temp.uData.uSize = { 4, 0 }; // !!!!! Is the size of pointer 4 ?
            temp.value = 0;
        }
    }
    return temp;
}
void recurseVariable(Variable* v, Variable* ref, void (*fp)(Variable*, Variable*)) {
    if (v->typeEnum == Variable::typeEnum_t::isRef) {
        fp(v, ref);
    }
    if (v->typeEnum == Variable::typeEnum_t::isArray) {
        for (auto it = v->array.begin(); it != v->array.end(); it++) {
            recurseVariable(&*it, ref, fp);
        }
    }
    if (v->typeEnum == Variable::typeEnum_t::isStruct) {
        for (auto it = v->intStruct.begin(); it != v->intStruct.end(); it++) {
            recurseVariable(&*it, ref, fp);
        }
    }
    fp(v, ref);
}
BbSize setVariableOffset(Variable* v, BbSize vOffset) {
    v->uData.myParent = myP;
    if (v->typeEnum == Variable::typeEnum_t::isArray) {
        v->uData.uSize = 0;
        v->uData.uOffset = vOffset;
        for (auto it = v->array.begin(); it != v->array.end(); it++) {
            vOffset = setVariableOffset(&*it, vOffset);
            v->uData.uSize += (&*it)->uData.uSize;
        }
    }
    else if (v->typeEnum == Variable::typeEnum_t::isStruct) {
        v->uData.uSize = 0;
        v->uData.uOffset = vOffset;
        for (auto it = v->intStruct.begin(); it != v->intStruct.end(); it++) {
            vOffset = setVariableOffset(&*it, vOffset);
            v->uData.uSize += (&*it)->uData.uSize;
        }
    }
    else if (v->typeEnum == Variable::typeEnum_t::isUnion) {
        v->uData.uSize = 0;
        for (auto it = v->intStruct.begin(); it != v->intStruct.end(); it++) {
            (&*it)->uData.uOffset = vOffset;
            setVariableOffset(&*it, vOffset);
            if (v->uData.uSize < (&*it)->uData.uSize) {
                v->uData.uSize = (&*it)->uData.uSize;
            }
        }
    }
    else {
        v->uData.uOffset = vOffset;
        vOffset += v->uData.uSize;
    }
    return vOffset;
}
void careUnions(Variable* v) {
    /* If v points to a union, update the parent address of all the members and 
    submembers.
    If v points to something which is part of a union, then updated the common area.
    */
    if (v->typeEnum == Variable::typeEnum_t::isUnion) {
        myP = v;
        setVariableOffset(v);
    }
    v->updateCommonArea();
}
void nullifyPointer(Variable* v, Variable* ref) {
    if (v->pointsTo == ref) {
        v->pointsTo = NULL;
    }
}
void cleanTestStorage() {
    VariableShadow::vVariable v;
    vShadowedVar.shadows.clear();
    vShadowedVar.shadows.push_back(v);
    vStruct.clear();
    vTypeDef.clear();
    vUnion.clear();
    vParams.clear();
    vEnum.clear();
    vFunction.clear();
    shadowLevel = 0;
}
void setSourceLocations(Node* tempNode, std::string str) {
    std::smatch smSourceRef;
    std::smatch smSourcePoint;
    std::string temp;
    std::regex_search(str, smSourceRef, eSourceRef);
    if (smSourceRef.size() >= 3 && smSourceRef[2].matched) {
        temp = smSourceRef[2];
        std::regex_search(temp, smSourcePoint, eSourcePoint);
        setGlobalLocation(smSourcePoint);
    }
    tempNode->sourceRef.ExtBegin.fileName = globalSource.fileName;
    tempNode->sourceRef.ExtBegin.line = globalSource.line;
    tempNode->sourceRef.ExtBegin.col = globalSource.col;
    if (smSourceRef.size() >= 4 && smSourceRef[3].matched) {
        temp = smSourceRef[3];
        std::regex_search(temp, smSourcePoint, eSourcePoint);
        setGlobalLocation(smSourcePoint);
    }
    tempNode->sourceRef.ExtEnd.fileName = globalSource.fileName;
    tempNode->sourceRef.ExtEnd.line = globalSource.line;
    tempNode->sourceRef.ExtEnd.col = globalSource.col;
    if (smSourceRef.size() >= 5 && smSourceRef[4].matched) {
        temp = smSourceRef[4];
        std::regex_search(temp, smSourcePoint, eSourcePoint);
        setGlobalLocation(smSourcePoint);
    }
    tempNode->sourceRef.Name.fileName = globalSource.fileName;
    tempNode->sourceRef.Name.line = globalSource.line;
    tempNode->sourceRef.Name.col = globalSource.col;
}
void setGlobalLocation(std::smatch smSourcePoint) {
    if (smSourcePoint[6].matched) {
        globalSource.fileName = smSourcePoint[7];
        globalSource.line = std::stoi(smSourcePoint[8]);
        globalSource.col = std::stoi(smSourcePoint[9]);
    }
    else if (smSourcePoint[3].matched) {
        globalSource.line = std::stoi(smSourcePoint[4]);
        globalSource.col = std::stoi(smSourcePoint[5]);
    }
    else if (smSourcePoint[1].matched) {
        globalSource.col = std::stoi(smSourcePoint[2]);
    }
}
unsigned long long interactionWGuiCore(Variable& ret, Node* node) {
    unsigned long long userValue = 0;

    Logger(std::string("requested user") + " " + ret.pointsTo->name + " " + __FILE__ + " " + std::to_string(__LINE__));
    forGui.line = node->sourceRef.ExtBegin.line - 1;
    forGui.col = node->sourceRef.ExtBegin.col;
    forGui.astLine = node->astFileRow;
    forGui.varName = ret.pointsTo->name;
    forGui.len = ret.pointsTo->name.length();
    forGui.strComm = "void";
    vf.writeFile();

    SetEvent(hEventReqGuiLine);
    //std::cout << "Enter value (source line " << node->sourceRef.Name.line  << "): ";
    WaitForSingleObject(hEventReqValue, INFINITE);
    //Sleep(2000);
    userValue = forGui.ValueFromGui;
    //std::cin >> userValue; 
    testRunner.freeRunning = true;
    ret.pointsTo->setByUser = true;
    ret.pointsTo->valueDoubleByUser = userValue;
    ret.pointsTo->valueByUser = userValue;
    return userValue;
}
bool interactionWGui(Variable& ret, Node* node)
{
    bool retVal = false;
    unsigned long long userValue = 0;
    bool localUsedInTest = false;
    localUsedInTest = ret.pointsTo->usedInTest;
    /* Local variable cannot be set by user */
    if (ret.pointsTo->braceNested > 0) localUsedInTest = true;
    /* Function may return a different value each time,
    so they are never set by uset and never use in test (?)
    They always interact with user */
    if (ret.pointsTo->typeEnum == Variable::typeEnum_t::isFunction) {
        if (testRunner.freeRunning == false) {
            /* Check if the functionReturns vector has other values to supply the test
            If there are no other values to supply, let's ask the user. */
            if (ret.pointsTo->funcRetIndex < ret.pointsTo->functionReturns.size()) {
                userValue = ret.pointsTo->functionReturns[ret.pointsTo->funcRetIndex].value;
                ret.pointsTo->value = ret.pointsTo->functionReturns[ret.pointsTo->funcRetIndex].value;
                ret.pointsTo->valueDouble = ret.pointsTo->functionReturns[ret.pointsTo->funcRetIndex].valueDouble;
                ret.pointsTo->funcRetIndex++;
            }
            else {
                Variable newFuncRet;
                userValue = interactionWGuiCore(ret, node);
                /* Update the function return main value because it will be used 
                to store the result in the valueCalls file */
                ret.pointsTo->value = userValue;
                ret.pointsTo->valueDouble = userValue;
                newFuncRet.value = userValue;
                newFuncRet.valueDouble = userValue;
                newFuncRet.ValueEnum = Variable::ValueEnum_t::isInteger;
                ret.pointsTo->functionReturns.push_back(newFuncRet);
                ret.pointsTo->funcRetIndex++;
                ret.pointsTo->setByUser = false;
                ret.pointsTo->usedInTest = false;
                retVal = true;
            }
        }
        else {
            userValue = ret.pointsTo->value = ret.pointsTo->valueByUser;
            userValue = ret.pointsTo->valueDouble = ret.pointsTo->valueDoubleByUser;
        }
    }
    else {
        if (localUsedInTest == false && testRunner.freeRunning == false) {
            if (ret.pointsTo->setByUser == true) {
                userValue = ret.pointsTo->value = ret.pointsTo->valueByUser;
                userValue = ret.pointsTo->valueDouble = ret.pointsTo->valueDoubleByUser;
            }
            else {
                userValue = interactionWGuiCore(ret, node);
                retVal = true;
            }
            ret.pointsTo->usedInTest = true;
            ret.pointsTo->value = userValue;
            ret.pointsTo->valueDouble = userValue;
        }
    }
    return retVal;
}
void buildCallInstance(Variable& call, std::vector<Variable> arguments) {
    auto itPar = call.pointsTo->intStruct.begin();
    auto itArg = arguments.begin();
    nlohmann::json j, jList, jFile;
    std::string prefix;
    std::string cantataCheckInstance;
    std::string cantataCall = "//Expected\n\"" + call.pointsTo->name + "\"\t\"#";
    std::string cantataCallInstance =
        "s_r_" +
        std::to_string(call.pointsTo->value);

    prefix = "Calling function " + call.pointsTo->name + "\n";
    /*json*/ 
    j["name"] = call.pointsTo->name;
    prefix = "";
    for (; itPar != call.pointsTo->intStruct.end() && itArg != arguments.end(); itPar++, itArg++) {
        prefix += "\tArg " + itPar->name + " = " + "";
        itArg->print(prefix, "", false);
        prefix = "";
        cantataCallInstance += "_p_" + std::to_string(itArg->value);
        /*json*/ 
        nlohmann::json jTemp;
        jTemp[itPar->name] = itArg->value;
        j["arg"].push_back(jTemp);
    }
    /*json*/
    j["cantataCallInstance"] = cantataCallInstance;
    jList = "\t\"" + call.pointsTo->name + "\"\t\t\"#" + cantataCallInstance + ";\"\n";
    cantataCall += cantataCallInstance + "\"\n";
    cantataCall += "\tReturn = " + std::to_string(call.pointsTo->value) + "\n";
    j["return"] = call.pointsTo->value;
    j["log"] = cantataCall;
    /* Build Cantata IfInstance */
    if (call.pointsTo->functionHasBody == false) {
        /* No function with this name has a body, so the Cantata Drive will be a stub */
        cantataCheckInstance = "IF_INSTANCE(\n\"" + cantataCallInstance + "\"\n) {\n";
        itPar = call.pointsTo->intStruct.begin();
        itArg = arguments.begin();
        for (; itPar != call.pointsTo->intStruct.end() && itArg != arguments.end(); itPar++, itArg++) {
            cantataCheckInstance +=
                "CHECK_S_INT(" +
                itPar->name +
                ", " +
                std::to_string(itArg->value) +
                ");\n";
        }
        cantataCheckInstance += "return ";
        // If the function returns void don't add any value
        if (call.pointsTo->type.find("void") == std::string::npos) {
            cantataCheckInstance += std::to_string(call.pointsTo->value);
        }
        cantataCheckInstance += ";\n}\n\n\n";
        /*json*/
        j["StubInstance"] = cantataCheckInstance;
    }
    else {
        /* A function with this name has a body, so the Cantata Drive will be a replace wrap 
        For now, no call will be am after wrapper, all calls are stubbed or replaced.*/
        /* Before part  */
        cantataCheckInstance = "IF_INSTANCE(\n\"" + cantataCallInstance + "\"\n) {\n";
        itPar = call.pointsTo->intStruct.begin();
        itArg = arguments.begin();
        for (; itPar != call.pointsTo->intStruct.end() && itArg != arguments.end(); itPar++, itArg++) {
            cantataCheckInstance +=
                "CHECK_S_INT(" +
                itPar->name +
                ", " +
                std::to_string(itArg->value) +
                ");\n";
        }
        cantataCheckInstance +=
            "return REPLACE_WRAPPER;\n}\n\n\n";

        /*json*/
        j["BeforeWrapInstance"] = cantataCheckInstance;

        /* Replace part  */
        cantataCheckInstance = "IF_INSTANCE(\n\"" + cantataCallInstance + "\"\n) {\n";
        itPar = call.pointsTo->intStruct.begin();
        itArg = arguments.begin();
        for (; itPar != call.pointsTo->intStruct.end() && itArg != arguments.end(); itPar++, itArg++) {
            cantataCheckInstance +=
                "CHECK_S_INT(" +
                itPar->name +
                ", " +
                std::to_string(itArg->value) +
                ");\n";
        }
        cantataCheckInstance +=
            "return " +
            std::to_string(call.pointsTo->value) +
            ";\n}\n\n\n";
        /*json*/
        j["ReplaceWrapInstance"] = cantataCheckInstance;
    }

    /*json*/
    std::ifstream InJson = std::ifstream(testRunner.callsFile);
    try {
        InJson >> jFile;
        InJson.close();
    }
    catch (std::exception&) {
    }
    jFile["expectedCalls"]["singleInstances"].push_back(j);
    jFile["expectedCalls"]["list"].push_back(jList);
    std::ofstream OutJson = std::ofstream(testRunner.callsFile);
    OutJson << jFile.dump(4);
    OutJson.close();

}
void writeCantataTestFile() {
    std::vector<std::string> testFileVec;
    std::ifstream testFilesStr(testRunner.testFile);
    std::string str1;
    while (getline(testFilesStr, str1)) {
        testFileVec.push_back(str1);
    }
    testFilesStr.close();

    // Search begin of test function declaration
    for (auto str : testFileVec) {
        



    }





}

Variable getTypDef(std::string uType) {
    std::smatch smTypeDef;
    std::smatch smStructType;
    Variable ret;
    if (std::regex_search(uType, smTypeDef, eBuiltinTypes)) {
        // Built In type found
        ret.typeEnum = Variable::typeEnum_t::isValue;
        return ret;
    }
    for (auto t : vTypeDef) {
        if (t.name.compare(uType)) {
            return t;
        }
    }
    if (std::regex_search(uType, smStructType, eStructType)) {
        for (auto s : vStruct) {
            if (s.name.compare(smStructType[1])) {
                return s;
            }
        }
    }
    return ret;
}
Variable fVarDecl(Node* node) {
    Variable& temp = *(new Variable());
    std::string rawType;
    struct coreType_str CoreTypes;
    std::smatch smVarDecl;
    Variable* vback = NULL;
    std::regex_search(node->text, smVarDecl, eVarDecl);
    rawType = smVarDecl[2];
    /*
    Must extract the core type, example: struct s (*[3])[4]
    Core type is *[3]
    example: struct s (*(*[10])[3])[4]
    Core type is *[10]
    */
    CoreTypes.coreType = rawType; 
    CoreTypes.declaredConst = false;
    findCoreType(CoreTypes);
    /* In CoreTypes.typeChainA there's now the vector of the types. 
       An array is made as of [11], a pointer is written as *, 
      (Function pointers are not implemented)
      In CoreTypes.finalType there is the final, the "real" type. 
      Can be an inbuilt type, as int or char, or void(must be preceded by a pointer), 
      struct, union, or a typedef.
      We allocate new Variables only to the first pointer. The correctness of the code
      has already been checked by the compiler. */

    temp = buildVariable(CoreTypes, node);
    temp.name = smVarDecl[1];
    temp.type = rawType;
    temp.used = true;
    temp.braceNested = shadowLevel;
    temp.declaredConst = CoreTypes.declaredConst;
    /* Try to initialize the variable, but only for value type of variable 
    smVarDecl[5] is the cinit */
    /* Variable initializers. For the moment, only array and simple values are supported. */
    if (smVarDecl[5].compare("cinit")) {
        Variable init;
        if (node->child && node->child->astType.compare("IntegerLiteral") == 0) {
            init = visit(node->child);
            temp.value = init.value;
            temp.valueDouble = init.valueDouble;
            temp.ValueEnum = init.ValueEnum;
        }
        if (node->child && node->child->astType.compare("InitListExpr") == 0) {
            if (temp.typeEnum == Variable::typeEnum_t::isArray) {
                init = visit(node->child);
                temp.array = init.array;
                for (auto it = temp.array.begin(); it != temp.array.end(); it++) {
                    it->valueByUser = it->value;
                    it->valueDoubleByUser = it->valueDouble;
                }
                temp.value = init.value;
                temp.valueDouble = init.valueDouble;
                temp.ValueEnum = init.ValueEnum;
                temp.valueByUser = init.value;
                temp.valueDoubleByUser = init.valueDouble;
            }
        }
    }

/*
//    if (smGenericType[5].length() > 0) {
//        int arrSize = std::stoi(smGenericType[5]);
//        Variable a;
//        a.typeEnum = Variable::typeEnum_t::isValue;
//        if (smGenericType[3].length() > 0) {
//            // example: int *[3], can be a struct or not
//            a.typeEnum = Variable::typeEnum_t::isRef;
//        }
//        else if (smGenericType[1].compare("struct") == 0) {
//            // example: struct s[3], without the pointer
//            // Must take care of anonymous structs
//            std::string typeName = smGenericType[2];
//            if (std::regex_search(typeName, smStructTypeAnonymous, eStructTypeAnonymous))
//            {
//                typeName = smStructTypeAnonymous[1];
//            }
//            for (auto t : vStruct) {
//                if (t.name.compare(typeName) == 0) {
//                    a = t;
//                }
//            }
//        }
//        temp.typeEnum = Variable::typeEnum_t::isArray;
//        for (int i = 0; i < arrSize; i++) {
//            a.value = i;
//            temp.array.push_back(a);
//        }
//    }
//    else if (smGenericType[3].length() > 0) {
//        // example: int *, it's not an array, but a pointer
//        temp.typeEnum = Variable::typeEnum_t::isRef;
//    }
//    else if (smGenericType[1].length() > 0) {
//        // example: struct s, it's neither an array not a pointer, but a struct
        // Must take care of anonymous structs
//        std::string typeName = smGenericType[2];
//        if (std::regex_search(typeName, smStructTypeAnonymous, eStructTypeAnonymous))
//        {
//            typeName = smStructTypeAnonymous[1];
//        }
//        for (Variable v : vStruct) {
//            if (v.name.compare(typeName) == 0) {
//                temp = v;
//                break;
//            }
//        }
//        temp.typeEnum = Variable::typeEnum_t::isStruct;
//        temp.name = smVarDecl[1];
//    }
//    else {
//        // example: int, it's neither an array nor a pointer nor a struct, but a basic type
//        temp.typeEnum = Variable::typeEnum_t::isValue;
//        temp.value = 0;
//    }
*/

    /* Add Variable to store if it's not a free running test 
    Pay attention, only for globals. Since local variables are 
    deallocated each time, then always rebuild them. */
    if (testRunner.buildGlobals == true || shadowLevel > 0) {
        vShadowedVar.shadows[shadowLevel].push_back(&temp);
        vback = vShadowedVar.shadows[shadowLevel].back();
        vback->myAddressDebug = vback;
        myP = vback;
        setVariableOffset(vback);
        return *vback;
    }
    return Variable();
}
Variable fIntegerLiteral(Node* node) {
    Variable ret;
    std::smatch smIntegralType;
    std::regex_search(node->text, smIntegralType, eIntegralType);
    ret.value = std::stoll(smIntegralType[2]);
    ret.type = smIntegralType[1];
    //valueCast(smIntegralType[1], ret);
    //signExtend(&ret);
    ret.typeEnum = Variable::typeEnum_t::isValue;
    ret.ValueEnum = Variable::ValueEnum_t::isInteger;
    node->setValue(ret.value);
    return ret;
}
Variable fFloatingLiteral(Node* node) {
    Variable ret;
    std::smatch smIntegralType;
    std::regex_search(node->text, smIntegralType, eFloatLIteral);
    ret.valueDouble = std::stof(smIntegralType[2]);
    valueCast(smIntegralType[1], ret);
    ret.type = smIntegralType[1];
    ret.typeEnum = Variable::typeEnum_t::isValue;
    ret.ValueEnum = Variable::ValueEnum_t::isFloat;
    // Write node value
    node->setValueDouble(ret.valueDouble);
    return ret;
}
Variable fTypedefDecl(Node* node) {
    /* This is virtually the same as fVarDecl. 
    I prefer to keep it separated. */
    Variable temp;
    std::string rawType;
    struct coreType_str CoreTypes;
    std::smatch smTypeDef;
    std::smatch smInvalidSloc;
    Variable* vback = NULL;
    /* First try to filter all those <<invalid sloc>>
    which are added by the compiler itself.
    */
    if (std::regex_search(node->text, smInvalidSloc, std::regex("<<invalid\\ssloc>>"))) {
        return temp;
    }
    std::regex_search(node->text, smTypeDef, eTypeDef);
    rawType = smTypeDef[3];
    /*
    Must extract the core type, example: struct s (*[3])[4]
    Core type is *[3]
    example: struct s (*(*[10])[3])[4]
    Core type is *[10]
    */
    CoreTypes.coreType = rawType;
    findCoreType(CoreTypes);
    /* In CoreTypes.typeChainA there's now the vector of the types.
       An array is made as of [11], a pointer is written as *,
      (Function pointers are not implemented)
      In CoreTypes.finalType there is the final, the "real" type.
      Can be an inbuilt type, as int or char, or void(must be preceded by a pointer),
      struct, union, or a typedef.
      We allocate new Variables only to the first pointer. The correctness of the code
      has already been checked by the compiler. */

    temp = buildVariable(CoreTypes, node);
    temp.name = smTypeDef[2];
    temp.type = rawType;
    temp.used = true;

    /*
    //    if (smGenericType[5].length() > 0) {
    //        int arrSize = std::stoi(smGenericType[5]);
    //        Variable a;
    //        a.typeEnum = Variable::typeEnum_t::isValue;
    //        if (smGenericType[3].length() > 0) {
    //            // example: int *[3], can be a struct or not
    //            a.typeEnum = Variable::typeEnum_t::isRef;
    //        }
    //        else if (smGenericType[1].compare("struct") == 0) {
    //            // example: struct s[3], without the pointer
    //            // Must take care of anonymous structs
    //            std::string typeName = smGenericType[2];
    //            if (std::regex_search(typeName, smStructTypeAnonymous, eStructTypeAnonymous))
    //            {
    //                typeName = smStructTypeAnonymous[1];
    //            }
    //            for (auto t : vStruct) {
    //                if (t.name.compare(typeName) == 0) {
    //                    a = t;
    //                }
    //            }
    //        }
    //        temp.typeEnum = Variable::typeEnum_t::isArray;
    //        for (int i = 0; i < arrSize; i++) {
    //            a.value = i;
    //            temp.array.push_back(a);
    //        }
    //    }
    //    else if (smGenericType[3].length() > 0) {
    //        // example: int *, it's not an array, but a pointer
    //        temp.typeEnum = Variable::typeEnum_t::isRef;
    //    }
    //    else if (smGenericType[1].length() > 0) {
    //        // example: struct s, it's neither an array not a pointer, but a struct
            // Must take care of anonymous structs
    //        std::string typeName = smGenericType[2];
    //        if (std::regex_search(typeName, smStructTypeAnonymous, eStructTypeAnonymous))
    //        {
    //            typeName = smStructTypeAnonymous[1];
    //        }
    //        for (Variable v : vStruct) {
    //            if (v.name.compare(typeName) == 0) {
    //                temp = v;
    //                break;
    //            }
    //        }
    //        temp.typeEnum = Variable::typeEnum_t::isStruct;
    //        temp.name = smVarDecl[1];
    //    }
    //    else {
    //        // example: int, it's neither an array nor a pointer nor a struct, but a basic type
    //        temp.typeEnum = Variable::typeEnum_t::isValue;
    //        temp.value = 0;
    //    }
    */

    // Add TypeDef to store
    vTypeDef.push_back(temp);
    vback = &vTypeDef.back();
    vback->myAddressDebug = vback;
    myP = vback;
    setVariableOffset(vback);
    return temp;
}
Variable fDeclRefExpr(Node* node) {
    std::smatch smDeclRefExpr;
    std::smatch smDeclRefExprEnum;
    std::smatch smDeclRefExprParmVar;
    std::smatch smDeclRefExprFunction;
    Variable ret;
    if (std::regex_search(node->text, smDeclRefExpr, eDeclRefExpr)) {
        std::string varName = smDeclRefExpr[2];
        for (auto it = vShadowedVar.shadows.rbegin(); it != vShadowedVar.shadows.rend(); it++) {
            for (auto itit = it->begin(); itit != it->end(); itit++) {
                if ((**itit).name.compare(varName) == 0) {
                    ret.pointsTo = *itit;
                    ret.typeEnum = Variable::typeEnum_t::isRef;
                    break;
                }
            }
        }
    }
    else if (std::regex_search(node->text, smDeclRefExprEnum, eDeclRefExprEnum)) {
        /* Captures
        1: should always be int
        2: the hexID to search, there are homonymous
        3: the face name
        4: should be always int
        */
        std::string hexID = smDeclRefExprEnum[2];
        for (auto it = vEnum.begin(); it != vEnum.end(); it++) {
            for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
                if (itit->hexID.compare(hexID) == 0) {
                    /* Enum constants returns directly the value and
                    no reference. 
                    In the ast there will be no L to R value cast */
                    ret = *itit;
                    ret.typeEnum = Variable::typeEnum_t::isEnum;
                    break;
                }
            }
        }
    }
    else if (std::regex_search(node->text, smDeclRefExprParmVar, eDeclRefExprParmVar)) {
        /* Captures
        1: type
        2: hexID
        3: the face name
        4: type
        */
        std::string name = smDeclRefExprParmVar[3];
        
        for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
            if (testRunner.targetFunction.compare(it->name) == 0) {
                for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
                    if (itit->name.compare(name) == 0) {
                        ret.pointsTo = &*itit;
                        ret.typeEnum = Variable::typeEnum_t::isRef;
                        break;
                    }
                }
            }
        }
    }
    else if (std::regex_search(node->text, smDeclRefExprFunction, eDeclRefExprFunction)) {
        /* Captures
        1: type
        2: hexID
        3: the face name
        4: type
        */
        std::string name = smDeclRefExprFunction[3];
        for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
            if (it->name.compare(name) == 0) {
                ret.pointsTo = &*it;
                ret.typeEnum = Variable::typeEnum_t::isRef;
                break;
            }
        }
    }
    else {
        expThrow("DeclRefExpr failed");
    }
    // Write node result value
    /*  Can be the case where the test is in free running 
    but the variable was set by user. Then we want to show the user set 
    value before the L to R conversion. The conversion may even never happen 
    if the variable is to be assigned by a var = 1 kind of instruction */

    if (ret.pointsTo && 
        (ret.pointsTo->typeEnum == Variable::typeEnum_t::isEnum ||
         ret.pointsTo->typeEnum == Variable::typeEnum_t::isValue)) {
        if (ret.pointsTo->usedInTest == false && ret.pointsTo->setByUser) {
            if (ret.pointsTo->ValueEnum == Variable::ValueEnum_t::isInteger) {
                node->setValue(ret.pointsTo->valueByUser);
            }
            else {
                node->setValueDouble(ret.pointsTo->valueDoubleByUser);
            }
        }
        else {
            if (ret.pointsTo->ValueEnum == Variable::ValueEnum_t::isInteger) {
                node->setValue(ret.pointsTo->value);
            }
            else {
                node->setValueDouble(ret.pointsTo->valueDouble);
            }
        }
    }
    return ret;
}
Variable fImplicitCastExpr(Node* node) {
    Variable ret;
    std::smatch smImplicitCastExpr;
    std::regex_search(node->text, smImplicitCastExpr, eImplicitCastExpr);
    std::string castTo = smImplicitCastExpr[1];
    /// Must always be a pointer to variable
    ret = visit(node->child);
    if (smImplicitCastExpr[2].compare("ArrayToPointerDecay") == 0) {
        // Leave the pointer as it is, it's already a pointer to array.
    }
    else if (smImplicitCastExpr[2].compare("FunctionToPointerDecay") == 0) {
        // Leave the pointer as it is, it's already a pointer to function.
    }
    else if (smImplicitCastExpr[2].compare("LValueToRValue") == 0) {
        if (ret.pointsTo->typeEnum == Variable::typeEnum_t::isArray) {
            ret.pointsTo = &ret.pointsTo->array[ret.pointsTo->arrayIx];
        }
        interactionWGui(ret, node);
        ret = *ret.pointsTo;
    }
    else if (smImplicitCastExpr[2].compare("IntegralCast") == 0) {
        /*
        Skip casting for enums, it makes no sense ... :( ?
        */
        if ((ret.typeEnum == Variable::typeEnum_t::isEnum) == false) {
            valueCast(smImplicitCastExpr[1].str(), ret);
        }
        ret.type = smImplicitCastExpr[1];
        signExtend(&ret);
    }
    else if (smImplicitCastExpr[2].compare("IntegralToFloating") == 0)
    {
        ret.valueDouble = ret.value;
        ret.ValueEnum = Variable::ValueEnum_t::isFloat;
    }
    // Write node result value
    if (ret.ValueEnum == Variable::ValueEnum_t::isInteger) {
        node->setValue(ret.value);
    }
    else {
        node->setValueDouble(ret.valueDouble);
    }
    /* No cast should be necessary as the type is not changed by 
    ImplicitCastExpr */
    return ret;
}
Variable fUnaryOperator(Node* node) {
    Variable opa, ret;
    std::smatch smUnaryOperator;
    std::regex_search(node->text, smUnaryOperator, eUnaryOperator);
    std::string castTo = smUnaryOperator[1];
    std::string fix = smUnaryOperator[2];
    std::string uoperator = smUnaryOperator[3];
    opa = visit(node->child);
    if (uoperator.compare("++") == 0) {
        if (fix.compare("postfix") == 0) {
            interactionWGui(opa, node);
            ret = *opa.pointsTo;
            /* Doesn't apply to float ? */
            opa.pointsTo->value++;
        }
        if (fix.compare("prefix") == 0) {
            /* Doesn't apply to float ? */
            interactionWGui(opa, node);
            opa.pointsTo->value++;
            ret = *opa.pointsTo;
        }
    }
    if (uoperator.compare("--") == 0) {
        if (fix.compare("postfix") == 0) {
            interactionWGui(opa, node);
            ret = *opa.pointsTo;
            /* Doesn't apply to float ? */
            opa.pointsTo->value--;
        }
        if (fix.compare("prefix") == 0) {
            /* Doesn't apply to float ? */
            interactionWGui(opa, node);
            opa.pointsTo->value--;
            ret = *opa.pointsTo;
        }
    }
    if (uoperator.compare("&") == 0) {
        ret = opa;
    }
    if (uoperator.compare("*") == 0) {
        ret = opa;
        ret.typeEnum = Variable::typeEnum_t::isRef;
    }
    return ret;
}
Variable fBinaryOperator(Node* node) {
    std::smatch smBinaryOperator;
    std::regex_search(node->text, smBinaryOperator, eBinaryOperator);
    std::string castTo = smBinaryOperator[1];
    std::string boperator = smBinaryOperator[2];
    Variable opa, opb;
    Variable ret;
#pragma warning Prevent side effect here, read comment
    /*TODO 
    Read opb or opa, only if needed in case of || && operators that can short circuit
    The value and stop the condition check
    */
    opa = visit(node->child);
    opb = visit(node->child->nextSib);
    ret = opa;
    if (boperator.compare("=") == 0) {
        /*
        std::string saveName = opa.pointsTo->name;
        Variable* saveParent = opa.pointsTo->uData.myParent;
        struct unionData saveUData = opa.pointsTo->uData;
        bool setByUser = opa.pointsTo->setByUser;
        unsigned long long valueByUser = opa.pointsTo->valueByUser;
        float valueDoubleByUser = opa.pointsTo->valueDoubleByUser;
        */
        opa.pointsTo->value = opb.value;
        opa.pointsTo->valueDouble = opb.valueDouble;
        opa.pointsTo->usedInTest = true;
        /*
        opa.pointsTo->uData.myParent = saveParent;
        opa.pointsTo->name = saveName;
        opa.pointsTo->uData = saveUData;
        opa.pointsTo->setByUser = setByUser;
        opa.pointsTo->valueByUser = valueByUser;
        opa.pointsTo->valueDoubleByUser = valueDoubleByUser;
        */
        careUnions(opa.pointsTo);
        ret = *opa.pointsTo;
    }
    else if (boperator.compare("<") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.value = opa.valueDouble < opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = (signed long long)opa.value < (signed long long)opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("<=") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.value = opa.valueDouble <= opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value <= opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare(">") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.value = opa.valueDouble > opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value > opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare(">=") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.value = opa.valueDouble >= opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value >= opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("==") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.value = opa.valueDouble == opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value == opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("!=") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.value = opa.valueDouble != opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value != opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("+") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.valueDouble = opa.valueDouble + opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value + opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("-") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.valueDouble = opa.valueDouble - opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value - opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("*") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.valueDouble = opa.valueDouble * opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value * opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("/") == 0) {
        if (opb.ValueEnum == Variable::ValueEnum_t::isFloat) {
            ret.valueDouble = opa.valueDouble / opb.valueDouble;
        }
        else if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
            ret.value = opa.value / opb.value;
        }
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("<<") == 0) {
        ret.value = opa.value << opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare(">>") == 0) {
        ret.value = opa.value >> opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("&") == 0) {
        ret.value = opa.value & opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("||") == 0) {
        ret.value = opa.value || opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else if (boperator.compare("&&") == 0) {
        ret.value = opa.value && opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
    }
    else {
        throw std::string("Unknown binary operator at ast row " + std::to_string(node->astFileRow) + " " + node->text);
    }
    // Write node result value
    if (opb.ValueEnum == Variable::ValueEnum_t::isInteger) {
        node->setValue(ret.value);
    }
    else {
        node->setValueDouble(ret.valueDouble);
    }
    return ret;
}
Variable fFieldDecl(Node* node) {
    Variable vField;
    std::smatch smFieldDeclaration;
    std::smatch smFieldDeclarationImplicit;
    std::smatch smStructTypeAnonymous;
    std::smatch smStructTypeAnonymous2;
    std::smatch smUnionTypeAnonymous;
    std::smatch smUnionTypeAnonymous2;
    std::smatch smStructType;
    std::string rawType;
    std::string name;
    std::string referenced;
    struct coreType_str coreTypes;
    bool found;
    if (std::regex_search(node->text, smFieldDeclarationImplicit, eFieldDeclarationImplicit)) {
        // Capture 1: "referenced" (or void)
        // Capture 2: type (will be anonymous)
        referenced = smFieldDeclarationImplicit[1];
        rawType = smFieldDeclarationImplicit[2];
    }
    else if (std::regex_search(node->text, smFieldDeclaration, eFieldDeclaration)) {
        // Capture 1: "referenced" (or void)
        // Capture 2: field name
        // Capture 3: type
        referenced = smFieldDeclaration[1];
        name = smFieldDeclaration[2];
        rawType = smFieldDeclaration[3];
    }
    if (std::regex_search(rawType, smStructTypeAnonymous, eStructTypeAnonymous)) {
        rawType = "struct " + smStructTypeAnonymous[1].str();
    }
    else if (std::regex_search(rawType, smStructTypeAnonymous2, eStructTypeAnonymous2)) {
        rawType = "struct " + smStructTypeAnonymous2[1].str();
    }
    else if (std::regex_search(rawType, smUnionTypeAnonymous, eUnionTypeAnonymous)) {
        rawType = "union " + smUnionTypeAnonymous[1].str();
    }
    else if (std::regex_search(rawType, smUnionTypeAnonymous2, eUnionTypeAnonymous2)) {
        rawType = "union " + smUnionTypeAnonymous2[1].str();
    }
    coreTypes.coreType = rawType;
    findCoreType(coreTypes);
    vField = buildVariable(coreTypes, node);
    vField.name = name;
    // Note the blank space in " referenced"
    if (smFieldDeclaration[1].compare(" referenced") == 0) {
        vField.used = true;
    }
    /* Search for bitfields. In case of bitfields the length will be that specified 
    by the bitfields.
    */
    if (node->child) {
        Variable bitField;
        bitField = visit(node->child);
        vField.uData.uSize = BbSize(0, bitField.value);
    }


    return vField;
}
Variable fRecordDecl(Node* node) {
    /*
    This function is for struct and for unions as well !!!
    */
    Variable tStruct, vField;
    std::smatch smStructDefinition;
    std::regex_search(node->text, smStructDefinition, eStructDefinition);
    /* Capture 1 is the hexadecimal ID of the node
       Capture 2 is struct or union 
       Capture 3 is the name of the struct */
    // Anonimous structures will take name as file.ext:line:col
    tStruct.name = smStructDefinition[3];
    /* The hexID will take a hexadecimal value
    It is necessary because when you make a typedef out of an 
    anonymous struct, the only way to find the struct is by its
    hexID */
    tStruct.hexID = smStructDefinition[1]; 
    if (tStruct.name.length() == 0) {
        tStruct.name =
            node->sourceRef.Name.fileName +
            ":" + std::to_string(node->sourceRef.Name.line) +
            ":" + std::to_string(node->sourceRef.Name.col);
    }
    if (smStructDefinition[2].compare("struct") == 0) {
        tStruct.typeEnum = Variable::typeEnum_t::isStruct;
    }
    else if (smStructDefinition[2].compare("union") == 0) {
        tStruct.typeEnum = Variable::typeEnum_t::isUnion;
        /* As we're allocating a union, we have to foresee the 
        common area. Initialize it */
        for (int i = 0; i < 256; i++)
            tStruct.intUnion.push_back(0x00);
    }
    for (auto n = node->child; n != NULL; n = n->nextSib) {
        vField = visit(n);
        if (n->astType.compare("FieldDecl") == 0) {
            tStruct.intStruct.push_back(vField);
            if (tStruct.typeEnum == Variable::typeEnum_t::isStruct) {
                tStruct.uData.uSize += vField.uData.uSize;
            }
            if (tStruct.typeEnum == Variable::typeEnum_t::isUnion) {
                // Make unione size the biggest of its memebrs
                if (tStruct.uData.uSize < vField.uData.uSize) {
                    tStruct.uData.uSize = vField.uData.uSize;
                }
            }
        }
    }
    if (smStructDefinition[2].compare("struct") == 0) {
        vStruct.push_back(tStruct);
    }
    else if (smStructDefinition[2].compare("union") == 0) {
        vUnion.push_back(tStruct);
    }
    return tStruct;
}
Variable fIndirectFieldDecl(Node* node) {
    return Variable();
}
Variable fMemberExpr(Node* node) {

    Variable v, ret;
    std::string memberName;
    std::smatch smMemberExpr;
    std::regex_search(node->text, smMemberExpr, eMemberExpr);
    memberName = smMemberExpr[3];
    v = visit(node->child);
    if (v.pointsTo->typeEnum == Variable::typeEnum_t::isStruct ||
        v.pointsTo->typeEnum == Variable::typeEnum_t::isUnion) {
        for (auto m = v.pointsTo->intStruct.begin(); m != v.pointsTo->intStruct.end(); m++) {
            if (memberName.compare(m->name) == 0) {
                ret.pointsTo = &*m;
                ret.typeEnum = Variable::typeEnum_t::isRef;
                // Always return a pointer because Memeber Expression is a lvalue
                return ret;
            }
        }
    }
    else {
        std::cout << "Error referencing variable is not a struct.";
    }
}
Variable fIfStmt(Node* node) {
    Variable cond, vtrue, vfalse;
    Node* node2;
    node2 = node->child;
    while (node2->astType.compare("<<<NULL") == 0) {
        node2 = node2->nextSib;
    }
    cond = visit(node2);
    if (cond.value) {
        if (node2->nextSib) {
            vtrue = visit(node2->nextSib);
            if (vtrue.ValueEnum == Variable::ValueEnum_t::isInteger) node->setValue(vtrue.value);
            else node->setValueDouble(vtrue.valueDouble);
            return vtrue;
        }
    }
    else {
        if (node2->nextSib->nextSib) {
            vfalse = visit(node2->nextSib->nextSib);
            if (vfalse.ValueEnum == Variable::ValueEnum_t::isInteger) node->setValue(vfalse.value);
            else node->setValueDouble(vfalse.valueDouble);
            return vfalse;
        }
    }
    return Variable();
}
Variable fForStmt(Node* node) {

    Node* init, * cond, * iter, * stmt;
    Variable vinit, vcond, viter, vstmt;
    init = node->child;
    cond = node->child->nextSib->nextSib;
    iter = node->child->nextSib->nextSib->nextSib;
    stmt = node->child->nextSib->nextSib->nextSib->nextSib;
    vinit = visit(init);
    while ((vcond = visit(cond)).value) {
        vstmt = visit(stmt);
        viter = visit(iter);
    }
    return Variable();
}
Variable fCompoundStmt(Node* node) {
    Variable temp;
    Variable ret;
    VariableShadow::vVariable v;
    VariableShadow::vVariable* vVar;
    shadowLevel++;
    vShadowedVar.shadows.push_back(v);
    for (auto next = node->child; next != NULL; next = next->nextSib) {
        Logger(next->text);
        temp = visit(next);
        if (temp.returnSignalled == true) break;
    }
    vVar = &vShadowedVar.shadows.back();
    /* For each variable that will go out of scope it searches if there are 
    pointer that points to it and will set them to NULL.
    Otherwise during e.g. the printing of the variable, the procedure will 
    follow dangling pointers.
    */
    for (auto var = vVar->begin(); var != vVar->end(); var++) {
        for (auto it = vShadowedVar.shadows.rbegin(); it != vShadowedVar.shadows.rend(); it++) {
            for (auto itit = it->begin(); itit != it->end(); itit++) {
                recurseVariable(*itit, *var, nullifyPointer);
            }
        }
    }
    vShadowedVar.shadows.pop_back();
    shadowLevel--;
    return temp;
}
Variable fTranslationUnitDecl(Node* node) {
    Variable temp;
    for (auto next = node->child; next != NULL; next = next->nextSib) {
        if ((next->astType.compare("FunctionDecl") == 0) 
            || (testRunner.buildGlobals == true)) {
            temp = visit(next);
        }
    }
    return Variable();
}
Variable fArraySubscriptExpr(Node* node) {
    Variable ret;
    Variable temp;
    Variable pArray = visit(node->child);
    int ix;
    temp = visit(node->child->nextSib);
    /* If the index comes from a variable it will be passed here as a pointer, 
    enums and other constant value must not be dereferenced. */
    if (temp.typeEnum == Variable::typeEnum_t::isRef) {
        ix = visit(node->child->nextSib).pointsTo->value;
    } 
    else {
        ix = visit(node->child->nextSib).value;
    }
    if (ix >= pArray.pointsTo->array.size()) {
        Logger(std::to_string(ix));
        throw std::string("Array index error " + std::to_string(ix) + " > " + std::to_string(pArray.pointsTo->array.size()));
    }
    ret.pointsTo = &pArray.pointsTo->array[ix];
    ret.typeEnum = Variable::typeEnum_t::isRef;
    ret.declaredConst = pArray.pointsTo->declaredConst;
    ret.setByUser = pArray.pointsTo->setByUser;
    ret.usedInTest = pArray.pointsTo->usedInTest;
    // Write node result value 
    node->setValue(ret.pointsTo->value);
    node->setValueDouble(ret.pointsTo->valueDouble);
    return ret;
}
Variable fFunctionDecl(Node* node) {
    Variable temp;
    Variable func;
    struct coreType_str CoreTypes;
    Node* node2;
    bool hasBody = false;
    bool alreadyDeclared = false;
    std::smatch smFunctionDecl;
    std::regex_search(node->text, smFunctionDecl, eFunctionDecl);
    func.name = smFunctionDecl[1];
    func.typeEnum = Variable::typeEnum_t::isFunction;
    func.type = smFunctionDecl[2];
    /* Must find core types for the return value */
    CoreTypes.coreType = func.type;
    stripParametersFunction(CoreTypes);
    func.type = CoreTypes.core;

    /* Search if funtion was already declared, even if only as a prototype */
    for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
        if (it->name.compare(func.name) == 0) {
            alreadyDeclared = true;
        }
    }
    /* This is the case where we are building the global variables and functions
    Since functions can be declared more than once, we must search it the declaration
    has already happend.
    We should check also for functions overloaded, with same name but different parameters,
    but we don't here. */
    if (testRunner.buildGlobals == true && alreadyDeclared == false) {
        if (node->child) {
            node2 = node->child;
            while (node2) {
                if (node2->astType.compare("ParmVarDecl") == 0) {
                    try {
                        temp = visit(node2);
                    }
                    catch (Variable v) {
                        v;
                    }
                    catch (std::string str) {
                        std::cout << str;
                        throw;
                    }
                    func.intStruct.push_back(temp);
                }
                node2 = node2->nextSib;
            }
        }
        Variable retValue; 
        retValue.name = "returnValue";
        func.intStruct.push_back(retValue);
        vFunction.push_back(func);
    }
    /* Update the functionHasBody flag, which will be useful to build instances for 
    stubs, or before wrappers and after wrappers. 
    At this point the function is declared */
    if (testRunner.buildGlobals == true) {
        if (node->child) {
            node2 = node->child;
            while (node2) {
                if (node2->astType.compare("CompoundStmt") == 0) {
                    for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                        if (it->name.compare(func.name) == 0) {
                            it->functionHasBody = true;
                            goto NodeLoop;
                        }
                    }
                }
                node2 = node2->nextSib;
            }
        NodeLoop:;
        }
    }
    /* This is the case where we are executing the target function, either in freerunning mode
    or in interactive mode.
    Here we don't care if function are declared more than once, since only one instance can have a
    body, and the ast compiler has already cared about it. */
    if (testRunner.buildGlobals == false && func.name.compare(testRunner.targetFunction) == 0) {
        if (node->child) {
            node2 = node->child;
            while (node2) {
                if (node2->astType.compare("CompoundStmt") == 0) {
                    try {
                        temp = visit(node2);
                        /* The value that comes out from the visit should be the return value
                        of the code under test. */
                        for (auto it = vFunction.begin(); it != vFunction.end(); it++) {
                            if (it->name.compare(func.name) == 0) {
                                for (auto itit = it->intStruct.begin(); itit != it->intStruct.end(); itit++) {
                                    if (itit->name.compare("returnValue") == 0) {
                                        temp.name = "returnValue";
                                        *itit = temp;
                                    }
                                }
                            }
                        }
                    }
                    catch (Variable v) {
                        v;
                    }
                    catch (std::string str) {
                        std::cout << str;
                        throw;
                    }
                }
                node2 = node2->nextSib;
            }
        }
    }
    vParams.clear();
    return temp;
}
Variable fDeclStmt(Node* node) {
    Variable temp;
    Node* node2; 
    node2 = node->child;
    while (node2) {
        temp = visit(node2);
        node2 = node2->nextSib;
    }
    return temp;
}
Variable fNULL(Node* node) {
    return Variable();
}
Variable fFullComment(Node* node) {
    return Variable();
}
Variable fConstantExpr(Node* node) {
    /* As simple as that */
    return visit(node->child);
}
Variable fEnumDecl(Node* node) {
    Variable ret;
    Variable temp;
    std::smatch smEnumDecl;
    if (!std::regex_search(node->text, smEnumDecl, eEnumDecl)) {
#if __GNUC__
        throw std::exception();
#else
        throw std::exception("EnumDecl cannot recognize regex parsing.");
#endif    
    }
    std::string hexID = smEnumDecl[1];
    std::string name = smEnumDecl[2];
    ret.hexID = hexID;
    ret.name = name;
    ret.typeEnum = Variable::typeEnum_t::isEnum;
    if (node->child) {
        Node* node2 = node->child;
        int progressive = 0;
        while (node2) {
            temp = visit(node2);
            // Skip comments
            if (temp.typeEnum == Variable::typeEnum_t::isEnum) {
                if (temp.value == 0x80000000) {
                    temp.value = progressive;
                }
                else {
                    progressive = temp.value;
                }
                progressive++;
                // Keep track of the name
                temp.type = name;
                ret.intStruct.push_back(temp);
            }
            node2 = node2->nextSib;
        }
    }
    vEnum.push_back(ret);
    return ret;
}
Variable fEnumConstantDecl(Node* node) {
    Variable ret;
    Variable temp;
    std::smatch smEnumConstantDecl;
    if (!std::regex_search(node->text, smEnumConstantDecl, eEnumConstantDecl)) {
#if __GNUC__
        throw std::exception();
#else
        throw std::exception("EnumConstantDecl cannot recognize regex parsing.");
#endif    
    }
    std::string hexID = smEnumConstantDecl[1];
    std::string name = smEnumConstantDecl[2];
    ret.hexID = hexID;
    ret.name = name;
    ret.typeEnum = Variable::typeEnum_t::isEnum;
    ret.type = "int";
    // This will cause the value to be reassigned progressively
    ret.value = 0x80000000;
    /* Check is the enum case value is forced to be assigned */
    if (node->child) {
        temp = visit(node->child);
        ret.value = temp.value;
    }
    return ret;

}
Variable fParmVarDecl(Node* node) {
/* Something as 
|-ParmVarDecl 0x2e6392983e0 <col:8, col:12> col:12 used a 'int'
*/
/* This function is really the same as VarDecl
However I like to keep them separated
*/
    Variable temp;
    std::smatch smParmVarDecl;
    std::string rawType;
    struct coreType_str CoreTypes;
    Variable* vback = NULL;
    std::regex_search(node->text, smParmVarDecl, eParmVarDecl);
    rawType = smParmVarDecl[2];
    /*
    Must extract the core type, example: struct s (*[3])[4]
    Core type is *[3]
    example: struct s (*(*[10])[3])[4]
    Core type is *[10]
    */
    CoreTypes.coreType = rawType;
    findCoreType(CoreTypes);
    /* In CoreTypes.typeChainA there's now the vector of the types.
       An array is made as of [11], a pointer is written as *,
      (Function pointers are not implemented)
      In CoreTypes.finalType there is the final, the "real" type.
      Can be an inbuilt type, as int or char, or void(must be preceded by a pointer),
      struct, union, or a typedef.
      We allocate new Variables only to the first pointer. The correctness of the code
      has already been checked by the compiler. */

    temp = buildVariable(CoreTypes, node);
    temp.name = smParmVarDecl[1];
    temp.type = rawType;
    temp.used = true;

    // Add Variable to store
    vParams.push_back(temp);
    vback = &vParams.back();
    vback->myAddressDebug = vback;
    myP = vback;
    setVariableOffset(vback);
    return *vback;

}
Variable fCallExpr(Node* node) {
    Variable call, arg;
    Node* node2;
    bool interactionOccour = false;
    std::vector<Variable> arguments;
    call = visit(node->child);
    node2 = node->child->nextSib;
    while (node2) {
        arg = visit(node2);
        arguments.push_back(arg);
        node2 = node2->nextSib;
    }
    /* For the moment, every function returns 100, 
    so to prevent error because of division by zero. */
    interactionOccour = interactionWGui(call, node);
    /* Calls must all be recorded because each time the return value may be different */
    if (interactionOccour) {
        buildCallInstance(call, arguments);
    }
    return *call.pointsTo;
}
Variable fParenExpr(Node* node) {
    Variable temp;
    for (auto next = node->child; next != NULL; next = next->nextSib) {
        temp = visit(next);
    }
    if (temp.ValueEnum == Variable::ValueEnum_t::isInteger) node->setValue(temp.value);
    else node->setValueDouble(temp.valueDouble);
    return temp;
}
Variable fReturnStmt(Node* node) {
    Variable retV;
    retV = visit(node->child);
    retV.usedInTest = true;
    retV.returnSignalled = true;
    return retV;
}
Variable fCStyleCastExprt(Node* node) {
    Variable ret;
    std::smatch smCStyleCastExprt;
    std::string firstType;
    std::string secondType;
    std::string typeOfCast;

    std::regex_search(node->text, smCStyleCastExprt, eCStyleCastExprt);
    firstType = smCStyleCastExprt[1];
    secondType = smCStyleCastExprt[2];
    typeOfCast = smCStyleCastExprt[3];
    if (secondType.length() == 0) {
        secondType = firstType;
    }
    if (typeOfCast.compare("IntegralCast") == 0) {
        ret = visit(node->child);
        valueCast(secondType, ret);
    }
    else if (typeOfCast.compare("ToVoid") == 0) {
        visit(node->child);
        ret = Variable();
    }
    else if (typeOfCast.compare("FloatingToIntegral") == 0) {
        ret = visit(node->child);
        ret.value = ret.valueDouble;
        ret.ValueEnum = Variable::ValueEnum_t::isInteger;
        valueCast(secondType, ret);
    }
    else if (typeOfCast.compare("NoOp") == 0) {
        ret = visit(node->child);
    }
    else if (typeOfCast.compare("FloatingCast") == 0) {
        ret = visit(node->child);
        valueCast(secondType, ret);
    }
    else if (typeOfCast.compare("IntegralToFloating") == 0) {
        ret = visit(node->child);
        ret.valueDouble = ret.value;
        ret.ValueEnum = Variable::ValueEnum_t::isFloat;
        valueCast(secondType, ret);
    }
    else {
        throw std::string("unknown cast to " + typeOfCast + " at row " + std::to_string(node->astFileRow) + " " + node->text);
    }
    // Write node result value
    if (ret.ValueEnum == Variable::ValueEnum_t::isInteger) {
        node->setValue(ret.value);
    }
    else {
        node->setValueDouble(ret.valueDouble);
    }
    return ret;

}
Variable fCompoundAssignOperator(Node* node) {
    std::smatch smCompoundAssignOperator;
    std::regex_search(node->text, smCompoundAssignOperator, eCompoundAssignOperator);
    std::string castTo = smCompoundAssignOperator[1];
    std::string boperator = smCompoundAssignOperator[2];
    Variable opa, opb;
    Variable ret;
    opa = visit(node->child);
    opb = visit(node->child->nextSib);
    ret = opa;
    if (boperator.compare("|=") == 0) {
        opa.pointsTo->value |= opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
        return ret;
    }
    else if (boperator.compare("&=") == 0) {
        opa.pointsTo->value &= opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
        return ret;
    }
    else if (boperator.compare("*=") == 0) {
        opa.pointsTo->value *= opb.value;
        ret.typeEnum = Variable::typeEnum_t::isValue;
        return ret;
    }
    else if (boperator.compare("/=") == 0) {
        if (opb.value == 0) {
            Logger("Divisione by zero in compound assignment");
        }
        else {
            opa.pointsTo->value /= opb.value;
            ret.typeEnum = Variable::typeEnum_t::isValue;
        }
        return ret;
    }
    else {
        Logger("Unrecognized compound operator " + boperator);
    }
}
Variable fWhileStmt(Node* node) {
    Variable cond, vtrue, vfalse;
    Node* node2;
    int iterationCount = 0;
    node2 = node->child;
    while (node2->astType.compare("<<<NULL") == 0) {
        node2 = node2->nextSib;
    }
    //cond = visit(node->child->nextSib->nextSib);  You may see this version <<NULL>>
    cond = visit(node2);
    while (cond.value) {
        if (node2->nextSib) {
            //vtrue = visit(node->child->nextSib->nextSib->nextSib); You may see this version <<NULL>>
            vtrue = visit(node2->nextSib);
        }
        iterationCount++; 
        /*
            !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
            Break on endless loops ?
        */
        if (iterationCount > 1000) break; 
    }
    return Variable();
}
Variable fConditionalOperator(Node* node) {
    Variable cond, vtrue, vfalse;
    Node* node2;
    //cond = visit(node->child->nextSib->nextSib);  You may see this version <<NULL>>
    node2 = node->child;
    while (node2->astType.compare("<<<NULL") == 0) {
        node2 = node2->nextSib;
    }
    cond = visit(node->child);
    if (cond.value) {
        if (node->child->nextSib)
            //vtrue = visit(node->child->nextSib->nextSib->nextSib); You may see this version <<NULL>>
            vtrue = visit(node->child->nextSib);
            if (vtrue.ValueEnum == Variable::ValueEnum_t::isInteger) node->setValue(vtrue.value);
            else node->setValueDouble(vtrue.valueDouble);
            return vtrue;
    }
    else {
        if (node->child->nextSib->nextSib)
            //vfalse = visit(node->child->nextSib->nextSib->nextSib->nextSib); You may see this version <<NULL>>
            vfalse = visit(node->child->nextSib->nextSib);
            if (vfalse.ValueEnum == Variable::ValueEnum_t::isInteger) node->setValue(vfalse.value);
            else node->setValueDouble(vfalse.valueDouble);
            return vfalse;
    }
    return Variable();
}
Variable fInitListExpr(Node* node) {
    Node* node2;
    Variable temp;
    Variable vArray;
    node2 = node->child;
    while (node2) {
        temp = visit(node2);
        vArray.array.push_back(temp);
        node2 = node2->nextSib;
    }
    return vArray;
}