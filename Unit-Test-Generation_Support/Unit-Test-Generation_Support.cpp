/*
TODO: Take care of local variables shading global variables. 
You have already see a strange case, that made you lose a lot of time in 
debugging. 
*/


#include <iostream>
#include <fstream>
#include <regex>
#include <iomanip>
#include <vector>
#include <string>
#include <set>
#include <utility>

#include "Variabile.h"
#include "CoreTypes.h"

struct SourceRef_s {
    std::string fileName;
    int line;
    int col;
};


class Node
{
public:
    Node();
    ~Node();
    Node* prevSib = NULL;
    Node* nextSib = NULL;
    Node* child = NULL;
    Node* parent = NULL;
    int depth;
    std::string text;
    std::string astType;
    struct SourceRef_s sourceRef;
};

Node::Node()
{
    depth = 0;
}

Node::~Node()
{
}


// vVariable contains all the global and local variables defined.
std::vector<Variable> vVariable;
// vStruct contains all the struct definition
std::vector<Variable> vStruct;
// vTypeDef contains all the user defined types
std::vector<Variable> vTypeDef;


Variable visit(Node* node);
unsigned long long cast(std::string str, unsigned long long v);

int main()
{
    std::cout << "Hello World!\n";
    if (std::regex_match("subject", std::regex("(sub)(.*)")))
        std::cout << "string literal matched\n";
    //std::string s("|-TypedefDecl 0xd2eaa8 <<invalid sloc>> <invalid sloc> implicit __NSConstantString 'struct __NSConstantString_tag");
    std::string s("[[[[aaaaa");
    std::regex e("(\\W*)");
    std::smatch sm;
    std::regex_search(s, sm, e);
    std::cout << sm[0] << "\n";
    std::cout << sm[1] << "\n";
    std::cout << sm[2] << "\n";

    std::ofstream pippo("pippo.txt");
    pippo << "pippo";
    pippo.close();

    std::ifstream infile("ex1/ast.txt");
    std::string str;
    std::regex eCatchGlobals    (R"(([^\w<]*)([\w<]+).*)");
    std::smatch smCatchGlobals;
    std::regex eCatchGlobalName (R"###([^\w<]*[\w<]+\s0x[\da-f]{6,7}\s<[^>]*>\s(?:col:\d+|line:\d+:\d+)(?:\sused)?\s(\w+)\s'([^']+)')###");
    std::smatch smCatchGlobalName;
    std::regex eSourceRef(
/*
|-RecordDecl 0x1b57f115d98 <temp.c:1:1, line:9:1> line:1:1 struct definition
A B         CD            E F         G H       I J       K
*/
/*A  */ "[^\\w<]*"
/*B 1*/ "([\\w<]+)"
/*C  */ "\\s"
/*D  */ "0x[\\da-f]{6,7}"
/*E  */ "\\s<"
/*F 2*/ "(col:\\d+|"
        "line:\\d+:\\d+|"
        "\\w+\\.\\w+:line:\\d+:\\d+)"
/*G  */ ",\\s"
/*H 3*/ "(col:\\d+|"
        "line:\\d+:\\d+|"
        "\\w+\\.\\w+:line:\\d+:\\d+)"
/*I  */ ">\\s"
/*J 4*/ "(col:\\d+|"
        "line:\\d+:\\d+|"
        "\\w+\\.\\w+:line:\\d+:\\d+)"
/*K  */ ".*"
);
    std::smatch smSourceRef;
    std::regex eCol("col:(\\d+)");
    std::smatch smCol;
    std::regex eLineCol("line:(\\d+)");
    std::smatch smLineCol;
    std::regex eFileLineCol("([\w\d]+)\.([\w\d]+):(\\d+):(\\d+)");
    std::smatch smFileLineCol;

    int depth = -2;
    Node myRoot;
    Node* cursor;
    cursor = &myRoot;
    struct SourceRef_s sr;
    //
    std::set<std::string> astTypesSet;

    while (std::getline(infile, str)) {
        if (std::regex_search(str, smCatchGlobals, eCatchGlobals) && smCatchGlobals.size() >= 2) {
            if  (   (   smCatchGlobals[1].length() == 2  // Will found only functions and global variables and skip local ones
                        && 
                            (smCatchGlobals[2].compare("VarDecl") == 0 
                            || 
                            smCatchGlobals[2].compare("FunctionDecl") == 0))
                    ||
                    (   smCatchGlobals[1].length() == 8
                        &&  // These are local variables
                        smCatchGlobals[2].compare("VarDecl") == 0)
                )
            {
                if (std::regex_search(str, smCatchGlobalName, eCatchGlobalName)) {
                    if (smCatchGlobals[1].length() == 2)
                        std::cout << "Found global  ";
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
            tempNode->astType = smCatchGlobals[2];
            // Get the complete source location
            tempNode->sourceRef = extractSourceRef(std::string str);


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
    for (auto it = astTypesSet.begin(); it != astTypesSet.end(); it++) {
        std::cout << *it << "\n";
    }
    /*
        Visit the AST
    */
    Variable t;
    t = visit(&myRoot);
}

Variable visit(Node *node)
{
    std::regex eIntegralType(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s'([^']+)'\s(\d+))###");
    std::smatch smIntegralType;
    std::regex eVarDecl(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s(?:col:\d+|line:\d+:\d+)(?:\sused)?\s(\w+)\s'([^']+)')###");
    std::smatch smVarDecl;
    std::regex eDeclRefExpr(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s'([^']+)':?(?:'(?:[^']+)')?\slvalue\sVar\s0x[\da-f]{6,11}\s'(\w+)')###");
    std::smatch smDeclRefExpr;
    std::regex eImplicitCastExpr(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s'([^']+)'\s<([^>]*)>)###");
    std::smatch smImplicitCastExpr;
    std::regex eBinaryOperator(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s'([^']+)'\s'([^']+)')###");
    std::smatch smBinaryOperator;
    std::regex eUnaryOperator(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s'([^']+)'(?:\slvalue)?\s(postfix|prefix)\s'([^']+)')###");
    std::smatch smUnaryOperator;
    std::regex eDeclStmt(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>)###");
    std::smatch smDeclStmt;
    std::regex eArrayDecl(R"###((.*)\[(\d+)\])###");
    std::smatch smArrayDecl;
    std::regex ePointerDecl(R"###(.*\s\*)###");
    std::smatch smPointerDecl;
    std::regex eStructType(R"###(struct\s([\w\d]+).*)###");
    std::smatch smStructType;
    std::regex eStructDefinition(R"###([^\w<]*[\w<]+\s0x[\da-f]{6,11}\s<[^>]*>\s(?:col:\d+|line:\d+:\d+)\sstruct\s(\w+)\sdefinition)###");
    std::smatch smStructDefinition;
    std::regex eFieldDeclaration(R"###([^\w<]*FieldDecl+\s0x[\da-f]{6,11}\s<[^>]*>\s(?:col:\d+|line:\d+:\d+)(\sreferenced)?\s(\w+)\s'([^']+)')###");
    std::smatch smFieldDeclaration;
    std::regex eMemberExpr(
/*
    | |-MemberExpr 0x26114378700 <col:5, col:8> 'int':'int' lvalue .a 0x26114378368
A-------B---------CD------------EF-------------GH----IJ----KL-----MNOPQ------------
*/
        "[^\\w<]*"          /* A Tree lines, connectors between lines inthe ast files           */ \
        "MemberExpr"        /* B Match excatly what you see                                     */ \
        "\\s"               /* C A space                                                        */ \
        "0x[\\da-f]{6,11}"  /* D The hex identifier of the ast node 0x123456                    */ \
        "\\s"               /* E A space                                                        */ \
        "<[^>]*>"           /* F Source location that is included in angular brackets <>        */ \
        "\\s"               /* G A space                                                        */ \
        "'([^']+)'"         /* H Type which this node return 'int'                              */ \
        ":?"                /* I A colon (for user types) 'mytype':'mytype'                     */ \
        "(?:'(?:[^']+)')?"  /* J The second 'mytype', see above                                 */ \
        "\\s"               /* K A space                                                        */ \
        "lvalue"            /* L matches exactly "lvalue"                                       */ \
        "\\s"               /* M A space                                                        */ \
        "(?:\\.|->)"        /* N mathes . or -> for members or pointers to members              */ \
        "([\\w\\d]+)"       /* O The member name which can have numbers after the first char    */ \
        "\\s"               /* P A space                                                        */ \
        "0x[\\da-f]{6,11}"  /* Q The hex identifier of the ast member node 0x123456             */ \
        );
    std::smatch smMemberExpr;
    /*
    Generic type regex
    capturing groups:
    1: union or struct or empty
    2: basic type or type or struct type
    3: indirection(pointer)
    4: nested type
    5: array size
    */
    std::regex eGenericType(R"###((union|struct|)?\s?([\w\d]+|)?\s?(\**)((?:\(.+\))?)\[?((?:\d+)?)\]?)###");
    std::smatch smGenericType;
    std::regex eTypeDef(
/*
                                                     1---------- 2  3--------
|-TypedefDecl 0x261143784f0 <line:8:1, col:19> col:19 referenced k 'struct st':'struct st'
A-B----------CD------------EF-----------------GH-----I----------JKLM----------
*/
        "[^\\w<]*"                     /* A Lines connecting the tree nodes in the ast file  */ \
        "TypedefDecl"                  /* B Matches what you see                             */ \
        "\\s"                          /* C A space                                          */ \
        "0x[\\da-f]{6,11}"             /* D The node hex identifier                          */ \
        "\\s"                          /* E A space                                          */ \
        "<[^>]*>"                      /* F Source file location                             */ \
        "\\s"                          /* G A space                                          */ \
        "(?:col:\\d+|line:\\d+:\\d+)"  /* H Source file exact location                       */ \
 /*1*/  "(\\sreferenced|)?"            /* I Might match what you see                         */ \
        "\\s"                          /* J A space                                          */ \
 /*2*/  "(\\w+)"                       /* K The typedef name                                 */ \
        "\\s"                          /* L A space                                          */ \
 /*3*/  "'([^']+)'"                    /* M The referenced type                              */ \
    );
    std::smatch smTypeDef;

    /*
        <<<NULL  (>>>)
        BinaryOperator
        BuiltinType
        CompoundStmt
        DeclRefExpr
        DeclStmt
        FunctionDecl
        IfStmt
        ImplicitCastExpr
        IntegerLiteral
        PointerType
        Record
        RecordType
        ReturnStmt
        TranslationUnitDecl
        TypedefDecl
        VarDecl
    */
    if (node->astType.compare("IntegerLiteral") == 0) {
        unsigned long long integral;
        Variable ret;
        std::regex_search(node->text, smIntegralType, eIntegralType);
        integral = std::stoi(smIntegralType[2]);
        ret.value = cast(smIntegralType[1], integral);
        return ret;
    }

    if (node->astType.compare("VarDecl") == 0) {
        Variable temp;
        std::string rawType, coreType;
        struct coreType_str CoreTypes;
        std::regex_search(node->text, smVarDecl, eVarDecl);
        temp.name = smVarDecl[1];
        rawType = smVarDecl[2];
        temp.type = rawType;
        temp.used = true;
        /*
        Must extract the core type, example: struct s (*[3])[4]
        Core type is *[3]
        example: struct s (*(*[10])[3])[4]
        Core type is *[10]
        */
        coreType = rawType;
        CoreTypes.coreType = rawType;
        findCoreType(CoreTypes);
        std::regex_search(CoreTypes.core, smGenericType, eGenericType);
        if (smGenericType[5].length() > 0) {
            int arrSize = std::stoi(smGenericType[5]);
            Variable a;
            a.typeEnum = Variable::typeEnum_t::isValue;
            if (smGenericType[3].length() > 0) {
                // example: int *[3], can be a struct or not
                a.typeEnum = Variable::typeEnum_t::isRef;
            } 
            else if (smGenericType[1].compare("struct") == 0) {
                // example: struct s[3], without the pointer
                for (auto t : vStruct) {
                    if (t.name.compare(smGenericType[2]) == 0) { 
                        a = t;
                    }
                }
            }
            temp.typeEnum = Variable::typeEnum_t::isArray;
            for (int i = 0; i < arrSize; i++) {
                a.value = i;
                temp.array.push_back(a);
            }
        }
        else if (smGenericType[3].length() > 0) {
            // example: int *, it's not an array, but a pointer
            temp.typeEnum = Variable::typeEnum_t::isRef;
        }
        else if (smGenericType[1].length() > 0) {
            // example: struct s, it's neither an array not a pointer, but a struct
            for (Variable v : vStruct) {
                if (v.name.compare(smGenericType[2]) == 0) {
                    temp = v;
                    break;
                }
            }
            temp.typeEnum = Variable::typeEnum_t::isStruct;
            temp.name = smVarDecl[1];
        }
        else {
            // example: int, it's neither an array nor a pointer nor a struct, but a basic type
            temp.typeEnum = Variable::typeEnum_t::isValue;
            temp.value = 0;
        }
        // Add Variable to store
        vVariable.push_back(temp);
        std::cout << "Value of variable " << temp.name << "\n";
        //std::cin >> temp.value;
        temp.value = 1;
        return temp;
    }

    if (node->astType.compare("TypedefDecl") == 0) {
        std::regex_search(node->text, smTypeDef, eTypeDef);
        std::string refed = smTypeDef[1];
        std::string name  = smTypeDef[2];
        std::string utype = smTypeDef[3];
    }

    if (node->astType.compare("DeclRefExpr") == 0) {
        std::regex_search(node->text, smDeclRefExpr, eDeclRefExpr);
        std::string varName = smDeclRefExpr[2];
        Variable ret;
        for (auto it = vVariable.begin(); it != vVariable.end(); it++) {
            if (it->name.compare(varName) == 0) {
                ret.pointsTo = &*it;
                ret.typeEnum = Variable::typeEnum_t::isRef;
                return ret;
            }
        }
    }

    if (node->astType.compare("ImplicitCastExpr") == 0) {
        Variable ret;
        std::regex_search(node->text, smImplicitCastExpr, eImplicitCastExpr);
        std::string castTo = smImplicitCastExpr[1];
        /// Must always be a pointer to variable
        ret = visit(node->child);
        if (smImplicitCastExpr[2].compare("ArrayToPointerDecay") == 0) {
            // Leave the pointer as it is, it's already a pointer to array.
            return ret;
        }
        if (smImplicitCastExpr[2].compare("LValueToRValue") == 0) {
            if (ret.typeEnum == Variable::typeEnum_t::isArray) {
                return ret.pointsTo->array[ret.pointsTo->arrayIx];
            }
            return *ret.pointsTo;
        }
        // No cast should be necessary as the type is not changed by 
        // ImplicitCastExpr
        return ret;
    }

    if (node->astType.compare("UnaryOperator") == 0) {
        Variable opa, ret;
        std::regex_search(node->text, smUnaryOperator, eUnaryOperator);
        std::string castTo = smUnaryOperator[1];
        std::string fix = smUnaryOperator[2];
        std::string uoperator = smUnaryOperator[3];
        opa = visit(node->child);
        if (uoperator.compare("++") == 0) {
            if (fix.compare("postfix") == 0) {
                ret = *opa.pointsTo;
            }
            opa.pointsTo->value++;
            if (fix.compare("prefix") == 0) {
                ret = *opa.pointsTo;
            }
        }
        if (uoperator.compare("--") == 0)  {
            if (fix.compare("postfix") == 0) {
                ret = *opa.pointsTo;
            }
            opa.pointsTo->value++;
            if (fix.compare("prefix") == 0) {
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

    if (node->astType.compare("BinaryOperator") == 0) {
        std::regex_search(node->text, smBinaryOperator, eBinaryOperator);
        std::string castTo = smBinaryOperator[1];
        std::string boperator = smBinaryOperator[2];
        Variable opa, opb;
        Variable ret;
        opa = visit(node->child);
        opb = visit(node->child->nextSib);
        if (boperator.compare("<") == 0) {
            ret.value = opa.value < opb.value;
            return ret;
        }
        if (boperator.compare("=") == 0) {
            std::string saveName = opa.pointsTo->name;
            *opa.pointsTo = opb;
            opa.pointsTo->name = saveName;
            ret = *opa.pointsTo;
        }
        return ret;
    }

    if (node->astType.compare("RecordDecl") == 0) {
        Variable tStruct, vField;
        std::regex_search(node->text, smStructDefinition, eStructDefinition);
        tStruct.name = smStructDefinition[1];
        tStruct.typeEnum = Variable::typeEnum_t::isStruct;
        for (auto n = node->child; n != NULL; n = n->nextSib) {
            vField = visit(n);
            tStruct.intStruct.push_back(vField);
        }
        vStruct.push_back(tStruct);
        return tStruct;
    }

    if (node->astType.compare("FieldDecl") == 0) {
        Variable vField;
        std::regex_search(node->text, smFieldDeclaration, eFieldDeclaration);
        // Capture 1: "referenced" (or void)
        // Capture 2: field name
        // Capture 3: type
        std::string rawType = smFieldDeclaration[3];
        if (std::regex_search(rawType, smStructType, eStructType)) {
            // Field is a nested struct
            for (Variable v : vStruct) {
                if (smStructType[1].compare(v.name) == 0) {
                    vField = v;
                }
            }
        }
        else {
            vField.typeEnum = Variable::typeEnum_t::isValue;
            vField.type = smFieldDeclaration[3];
        }
        vField.name = smFieldDeclaration[2];
        // Note the blank space in " referenced"
        if (smFieldDeclaration[1].compare(" referenced") == 0) {
            vField.used = true;
        }
        return vField;
    }

    if (node->astType.compare("MemberExpr") == 0) {
        Variable v, ret;
        std::string memberName;
        std::regex_search(node->text, smMemberExpr, eMemberExpr);
        memberName = smMemberExpr[2];
        v = visit(node->child);
        if (v.pointsTo->typeEnum == Variable::typeEnum_t::isStruct) {
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

    if (node->astType.compare("IfStmt") == 0) {
        Variable cond, vtrue, vfalse;
        cond = visit(node->child->nextSib->nextSib);
        if (cond.value) {
            vtrue = visit(node->child->nextSib->nextSib->nextSib);
        }
        else {
            vfalse = visit(node->child->nextSib->nextSib->nextSib->nextSib);
        }
        return Variable();
    }

    if (node->astType.compare("ForStmt") == 0) {
        Node *init, *cond, *iter, *stmt;
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

    if (node->astType.compare("CompoundStmt") == 0) {
        Variable temp;
        Variable ret;
        for (auto next = node->child; next != NULL; next = next->nextSib) {
            temp = visit(next);
        }
        return ret;
    }

    if (node->astType.compare("TranslationUnitDecl") == 0) {
        Variable temp;
        for (auto next = node->child; next != NULL; next = next->nextSib) {
            temp = visit(next);
        }
        return Variable();
    }

    if (node->astType.compare("ArraySubscriptExpr") == 0) {
        Variable ret;
        Variable pArray = visit(node->child);
        int ix = visit(node->child->nextSib).value;
        ret.pointsTo = &pArray.pointsTo->array[ix];
        ret.typeEnum = Variable::typeEnum_t::isRef;
        return ret;
    }

    if (node->astType.compare("FunctionDecl") == 0) {
        Variable temp = visit(node->child);
        for (auto v = vVariable.begin(); v != vVariable.end(); v++) {
            v->print();
        }
        return temp;
    }
    

    if (node->astType.compare("DeclStmt") == 0) {
        Variable temp = visit(node->child);
        return temp;
    }

    if (node->astType.compare("<<<NULL") == 0) {
        return Variable();
    }

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

unsigned long long cast(std::string str, unsigned long long v) 
{
    if (str.compare("int") == 0) {
        return (int)v;
    }
    if (str.compare("unsigned int") == 0) {
        return (unsigned int)v;
    }

    std::cout << "Exception encountered\n";
}
