//
//  StaticFunctionAnalyzer.cpp
//  Emojicode
//
//  Created by Theo Weidmann on 05/01/16.
//  Copyright © 2016 Theo Weidmann. All rights reserved.
//

#include "StaticFunctionAnalyzer.hpp"
#include "FileParser.hpp"
#include "Lexer.hpp"
#include "Type.hpp"
#include "utf8.h"
#include "Class.hpp"

std::vector<const Token *> stringPool;

Type StaticFunctionAnalyzer::parse(const Token *token, const Token *parentToken, Type type) {
    auto returnType = parse(token, parentToken);
    if (!returnType.compatibleTo(type, contextType)) {
        auto cn = returnType.toString(contextType, true);
        auto tn = type.toString(contextType, true);
        compilerError(token, "%s is not compatible to %s.", cn.c_str(), tn.c_str());
    }
    return returnType;
}

void StaticFunctionAnalyzer::noReturnError(const Token *errorToken){
    if (callable.returnType.type != TT_NOTHINGNESS && !returned) {
        compilerError(errorToken, "An explicit return is missing.");
    }
}

void StaticFunctionAnalyzer::noEffectWarning(const Token *warningToken){
    if(!effect){
        compilerWarning(warningToken, "Statement seems to have no effect whatsoever.");
    }
}

void StaticFunctionAnalyzer::checkAccess(Procedure *p, const Token *token, const char *type){
    if (p->access == PRIVATE) {
        if (contextType.type != TT_CLASS || p->eclass != contextType.eclass) {
            ecCharToCharStack(p->name, nm);
            compilerError(token, "%s %s is 🔒.", type, nm);
        }
    }
    else if(p->access == PROTECTED) {
        if (contextType.type != TT_CLASS || contextType.eclass->inheritsFrom(p->eclass)) {
            ecCharToCharStack(p->name, nm);
            compilerError(token, "%s %s is 🔐.", type, nm);
        }
    }
}

void StaticFunctionAnalyzer::checkArguments(Arguments arguments, Type calledType, const Token *token){
    bool brackets = false;
    if (nextToken()->type == ARGUMENT_BRACKET_OPEN) {
        consumeToken();
        brackets = true;
    }
    for (auto var : arguments) {
        parse(consumeToken(), token, var.type.resolveOn(calledType));
    }
    if (brackets) {
        consumeToken(ARGUMENT_BRACKET_CLOSE);
    }
}

void StaticFunctionAnalyzer::writeCoinForScopesUp(uint8_t scopesUp, const Token *varName, EmojicodeCoin stack, EmojicodeCoin object){
    if(scopesUp == 0){
        writer.writeCoin(stack);
    }
    else if(scopesUp == 1){
        writer.writeCoin(object);
        usedSelf = true;
    }
    else {
        compilerError(varName, "The variable cannot be resolved correctly.");
    }
}

uint8_t StaticFunctionAnalyzer::nextVariableID(){
    return variableCount++;
}

void StaticFunctionAnalyzer::flowControlBlock(){
    scoper.currentScope()->changeInitializedBy(1);
    if (!inClassContext) {
        scoper.topScope()->changeInitializedBy(1);
    }
    
    flowControlDepth++;
    
    const Token *token = consumeToken(IDENTIFIER);
    if (token->value[0] != E_GRAPES){
        ecCharToCharStack(token->value[0], s);
        compilerError(token, "Expected 🍇 but found %s instead.", s);
    }
    
    auto placeholder = writer.writeCoinsCountPlaceholderCoin();
    while (token = consumeToken(), !(token->type == IDENTIFIER && token->value[0] == E_WATERMELON)) {
        effect = false;
        parse(token, token);
        noEffectWarning(token);
    }
    placeholder.write();
    
    effect = true;
    
    scoper.currentScope()->changeInitializedBy(-1);
    if (!inClassContext) {
        scoper.topScope()->changeInitializedBy(-1);
    }
    
    flowControlDepth--;
    returned = false;
}

void StaticFunctionAnalyzer::parseIfExpression(const Token *token){
    if (nextToken()->value[0] == E_SOFT_ICE_CREAM) {
        consumeToken();
        writer.writeCoin(0x3E);
        
        const Token *varName = consumeToken(VARIABLE);
        if(scoper.currentScope()->getLocalVariable(varName) != nullptr){
            compilerError(token, "Cannot redeclare variable.");
        }
        
        uint8_t id = nextVariableID();
        writer.writeCoin(id);
        
        Type t = parse(consumeToken(), token);
        if (!t.optional) {
            compilerError(token, "🍊🍦 can only be used with optionals.");
        }
        
        t.optional = false;
        scoper.currentScope()->setLocalVariable(varName, new CompilerVariable(t, id, 1, true, varName));
    }
    else {
        parse(consumeToken(), token, typeBoolean);
    }
}

Type StaticFunctionAnalyzer::parse(const Token *token, const Token *parentToken) {
    if (token == nullptr) {
        compilerError(parentToken, "Unexpected end of function body.");
    }
    
    switch(token->type){
        case STRING: {
            writer.writeCoin(0x10);
            
            for (size_t i = 0; i < stringPool.size(); i++) {
                const Token *a = stringPool[i];
                if (a->value.compare(token->value) == 0) {
                    writer.writeCoin((EmojicodeCoin)i);
                    return Type(CL_STRING);
                }
            }
            
            writer.writeCoin((EmojicodeCoin)stringPool.size());
            stringPool.push_back(token);
            
            return Type(CL_STRING);
        }
        case BOOLEAN_TRUE:
            writer.writeCoin(0x11);
            return typeBoolean;
        case BOOLEAN_FALSE:
            writer.writeCoin(0x12);
            return typeBoolean;
        case INTEGER: {
            /* We know token->value only contains ints less than 255 */
            const char *string = token->value.utf8CString();
            
            EmojicodeInteger l = strtoll(string, nullptr, 0);
            delete [] string;
            if (llabs(l) > INT32_MAX) {
                writer.writeCoin(0x14);
                
                writer.writeCoin(l >> 32);
                writer.writeCoin((EmojicodeCoin)l);
                
                return typeInteger;
            }
            else {
                writer.writeCoin(0x13);
                writer.writeCoin((EmojicodeCoin)l);
                
                return typeInteger;
            }
        }
        case DOUBLE: {
            writer.writeCoin(0x15);
            
            const char *string = token->value.utf8CString();
            
            double d = strtod(string, nullptr);
            delete [] string;
            writer.writeDouble(d);
            return typeFloat;
        }
        case SYMBOL:
            writer.writeCoin(0x16);
            writer.writeCoin(token->value[0]);
            return typeSymbol;
        case VARIABLE: {
            uint8_t scopesUp;
            CompilerVariable *cv = scoper.getVariable(token, &scopesUp);
            
            if(cv == nullptr){
                const char *variableName = token->value.utf8CString();
                compilerError(token, "Variable \"%s\" not defined.", variableName);
            }
            
            cv->uninitalizedError(token);
            
            writeCoinForScopesUp(scopesUp, token, 0x1A, 0x1C);
            writer.writeCoin(cv->id);
            
            return cv->type;
        }
        case IDENTIFIER:
            return unsafeParseIdentifier(token);
        case DOCUMENTATION_COMMENT:
            compilerError(token, "Misplaced documentation comment.");
        case ARGUMENT_BRACKET_OPEN:
            compilerError(token, "Unexpected 〖");
        case ARGUMENT_BRACKET_CLOSE:
            compilerError(token, "Unexpected 〗");
        case NO_TYPE:
        case COMMENT:
            break;
    }
    compilerError(token, "Cannot determine expression’s return type.");
}

#define dynamismLevelFromSI() (inClassContext ? AllowDynamicClassType : NoDynamism)

Type StaticFunctionAnalyzer::unsafeParseIdentifier(const Token *token){
    if(token->value[0] != E_RED_APPLE){
        //We need a chance to test whether the red apple’s return is used
        effect = true;
    }

    switch (token->value[0]) {
        case E_SHORTCAKE: {
            const Token *varName = consumeToken(VARIABLE);
            
            if (scoper.currentScope()->getLocalVariable(varName) != nullptr) {
                compilerError(token, "Cannot redeclare variable.");
            }
            
            Type t = Type::parseAndFetchType(contextType, currentNamespace, dynamismLevelFromSI(), nullptr);
            
            uint8_t id = nextVariableID();
            scoper.currentScope()->setLocalVariable(varName, new CompilerVariable(t, id, t.optional ? 1 : 0, false, varName));
            
            return typeNothingness;
        }
        case E_CUSTARD: {
            const Token *varName = consumeToken(VARIABLE);
            
            uint8_t scopesUp;
            CompilerVariable *cv = scoper.getVariable(varName, &scopesUp);
            if(cv == nullptr){
                //Not declared, declaring as local variable
                
                writer.writeCoin(0x1B);
                
                uint8_t id = nextVariableID();
                
                writer.writeCoin(id);
                
                Type t = parse(consumeToken(), token);
                scoper.currentScope()->setLocalVariable(varName, new CompilerVariable(t, id, 1, false, varName));
            }
            else {
                if (cv->initialized <= 0) {
                    cv->initialized = 1;
                }
                
                cv->mutate(varName);
                
                writeCoinForScopesUp(scopesUp, varName, 0x1B, 0x1D);
                writer.writeCoin(cv->id);
                
                parse(consumeToken(), token, cv->type);
            }
            return typeNothingness;
        }
        case E_SOFT_ICE_CREAM: {
            const Token *varName = consumeToken(VARIABLE);
            
            if(scoper.currentScope()->getLocalVariable(varName) != nullptr){
                compilerError(token, "Cannot redeclare variable.");
            }
            
            writer.writeCoin(0x1B);
            
            uint8_t id = nextVariableID();
            writer.writeCoin(id);
            
            Type t = parse(consumeToken(), token);
            scoper.currentScope()->setLocalVariable(varName, new CompilerVariable(t, id, 1, true, varName));
            return typeNothingness;
        }
        case E_COOKING:
        case E_CHOCOLATE_BAR: {
            const Token *varName = consumeToken(VARIABLE);
            
            //Fetch the old value
            uint8_t scopesUp;
            CompilerVariable *cv = scoper.getVariable(varName, &scopesUp);
            
            if (!cv) {
                compilerError(token, "Unknown variable \"%s\"", varName->value.utf8CString());
                break;
            }
            
            cv->uninitalizedError(varName);
            cv->mutate(varName);
            
            if (!cv->type.compatibleTo(typeInteger, contextType)) {
                ecCharToCharStack(token->value[0], ls);
                compilerError(token, "%s can only operate on 🚂 variables.", ls);
            }
            
            if (token->value[0] == E_COOKING) { //decrement
                writeCoinForScopesUp(scopesUp, varName, 0x19, 0x1F);
            }
            else {
                writeCoinForScopesUp(scopesUp, varName, 0x18, 0x1E);
            }
            
            writer.writeCoin(cv->id);
            
            return typeNothingness;
        }
        case E_COOKIE: {
            writer.writeCoin(0x52);
            auto placeholder = writer.writeCoinPlaceholder();
            
            uint32_t stringCount = 1;
            
            parse(consumeToken(), token, Type(CL_STRING));
            
            const Token *stringToken;
            while (stringToken = consumeToken(), !(stringToken->type == IDENTIFIER && stringToken->value[0] == E_COOKIE)) {
                parse(stringToken, token, Type(CL_STRING));
                stringCount++;
            }
            
            placeholder.write(stringCount);
            return Type(CL_STRING);
        }
        case E_ICE_CREAM: {
            writer.writeCoin(0x51);
            
            auto placeholder = writer.writeCoinsCountPlaceholderCoin();
            
            CommonTypeFinder ct;
            
            const Token *aToken;
            while (aToken = consumeToken(), !(aToken->type == IDENTIFIER && aToken->value[0] == E_AUBERGINE)) {
                ct.addType(parse(aToken, token), contextType);
            }
            
            placeholder.write();
            
            Type type = Type(CL_LIST);
            type.genericArguments[0] = ct.getCommonType(token);
            
            return type;
        }
        case E_HONEY_POT: {
            writer.writeCoin(0x50);
           
            auto placeholder = writer.writeCoinsCountPlaceholderCoin();
            
            CommonTypeFinder ct;
            
            const Token *aToken;
            while (aToken = consumeToken(), !(aToken->type == IDENTIFIER && aToken->value[0] == E_AUBERGINE)) {
                parse(aToken, token, Type(CL_STRING));
                ct.addType(parse(consumeToken(), token), contextType);
            }
            
            placeholder.write();
            
            Type type = Type(CL_DICTIONARY);
            type.genericArguments[0] = ct.getCommonType(token);
            
            return type;
        }
        case E_TANGERINE: { //MARK: If
            writer.writeCoin(0x62);
            
            auto placeholder = writer.writeCoinsCountPlaceholderCoin();
            
            parseIfExpression(token);
            
            flowControlBlock();
            
            while ((token = nextToken()) != nullptr && token->type == IDENTIFIER && token->value[0] == E_LEMON) {
                writer.writeCoin(consumeToken()->value[0]);
                
                parseIfExpression(token);
                flowControlBlock();
            }
            
            if((token = nextToken()) != nullptr && token->type == IDENTIFIER && token->value[0] == E_STRAWBERRY){
                writer.writeCoin(consumeToken()->value[0]);
                flowControlBlock();
            }
            
            placeholder.write();
            
            return typeNothingness;
        }
        case E_CLOCKWISE_RIGHTWARDS_AND_LEFTWARDS_OPEN_CIRCLE_ARROWS: {
            writer.writeCoin(0x61);
            
            parse(consumeToken(), token, typeBoolean);
            flowControlBlock();
            
            return typeNothingness;
        }
        case E_CLOCKWISE_RIGHTWARDS_AND_LEFTWARDS_OPEN_CIRCLE_ARROWS_WITH_CIRCLED_ONE_OVERLAY: {
            auto placeholder = writer.writeCoinPlaceholder();
            
            const Token *variableToken = consumeToken(VARIABLE);
            
            if (scoper.currentScope()->getLocalVariable(variableToken) != nullptr) {
                compilerError(variableToken, "Cannot redeclare variable.");
            }
            
            uint8_t vID = nextVariableID();
            writer.writeCoin(vID);
            //Internally needed
            writer.writeCoin(nextVariableID());
            
            Type iteratee = parse(consumeToken(), token, typeSomeobject);
            
            if(iteratee.type == TT_CLASS && iteratee.eclass == CL_LIST) {
                //If the iteratee is a list, the Real-Time Engine has some special sugar
                placeholder.write(0x65);
                scoper.currentScope()->setLocalVariable(variableToken, new CompilerVariable(iteratee.genericArguments[0], vID, true, true, variableToken));
            }
            else if(iteratee.compatibleTo(Type(PR_ENUMERATEABLE, false), contextType)) {
                placeholder.write(0x64);
                Type itemType = typeSomething;
                if(iteratee.type == TT_CLASS && iteratee.eclass->ownGenericArgumentCount == 1) {
                    itemType = iteratee.genericArguments[iteratee.eclass->ownGenericArgumentCount - iteratee.eclass->genericArgumentCount];
                }
                scoper.currentScope()->setLocalVariable(variableToken, new CompilerVariable(itemType, vID, true, true, variableToken));
            }
            else {
                auto iterateeString = iteratee.toString(contextType, true);
                compilerError(token, "%s does not conform to 🔴🔂.", iterateeString.c_str());
            }
            
            flowControlBlock();
            
            return typeNothingness;
        }
        case E_LEMON:
        case E_STRAWBERRY:
        case E_WATERMELON:
        case E_AUBERGINE:
        case E_RAT: {
            ecCharToCharStack(token->value[0], identifier);
            compilerError(token, "Unexpected identifier %s.", identifier);
            return typeNothingness;
        }
        case E_DOG: {
            usedSelf = true;
            writer.writeCoin(0x3C);
            if (initializer && !calledSuper && initializer->eclass->superclass) {
                compilerError(token, "Attempt to use 🐕 before superinitializer call.");
            }
            
            if (inClassContext) {
                compilerError(token, "Illegal use of 🐕.", token->value[0]);
                break;
            }
            
            scoper.topScope()->initializerUnintializedVariablesCheck(token, "Instance variable \"%s\" must be initialized before the use of 🐕.");
            
            return contextType;
        }
        case E_UP_POINTING_RED_TRIANGLE: {
            writer.writeCoin(0x13);
            
            Type type = Type::parseAndFetchType(contextType, currentNamespace, dynamismLevelFromSI(), nullptr);
            
            if (type.type != TT_ENUM) {
                compilerError(token, "The given type cannot be accessed.");
            }
            else if (type.optional) {
                compilerError(token, "Optionals cannot be accessed.");
            }
            
            auto name = consumeToken(IDENTIFIER);
            
            auto v = type.eenum->getValueFor(name->value[0]);
            if (!v.first) {
                ecCharToCharStack(name->value[0], valueName);
                ecCharToCharStack(type.eenum->name, enumName);
                compilerError(name, "%s does not have a member named %s.", enumName, valueName);
            }
            else if (v.second > UINT32_MAX) {
                writer.writeCoin(v.second >> 32);
                writer.writeCoin((EmojicodeCoin)v.second);
            }
            else {
                writer.writeCoin((EmojicodeCoin)v.second);
            }
            
            return type;
        }
        case E_LARGE_BLUE_DIAMOND: {
            writer.writeCoin(0x4);
            
            bool dynamic;
            Type type = Type::parseAndFetchType(contextType, currentNamespace, dynamismLevelFromSI(), &dynamic);
            
            if (type.type != TT_CLASS) {
                compilerError(token, "The given type cannot be initiatied.");
            }
            else if (type.optional) {
                compilerError(token, "Optionals cannot be initiatied.");
            }
            
            if (dynamic) {
                writer.writeCoin(UINT32_MAX);
            }
            else {
                writer.writeCoin(type.eclass->index);
            }
            
            //The initializer name
            const Token *name = consumeToken(IDENTIFIER);
            
            Initializer *initializer = type.eclass->getInitializer(name->value[0]);
            
            if (initializer == nullptr) {
                auto typeString = type.toString(contextType, true);
                ecCharToCharStack(name->value[0], initializerString);
                compilerError(name, "%s has no initializer %s.", typeString.c_str(), initializerString);
            }
            else if (dynamic && !initializer->required) {
                compilerError(name, "Only required initializers can be used with 🐀.");
            }
            
            writer.writeCoin(initializer->vti);
            
            checkAccess(initializer, token, "Initializer");
            checkArguments(initializer->arguments, type, token);
            
            if (initializer->canReturnNothingness) {
                type.optional = true;
            }
            return type;
        }
        case E_HIGH_VOLTAGE_SIGN: {
            writer.writeCoin(0x17);
            return typeNothingness;
        }
        case E_CLOUD: {
            writer.writeCoin(0x2E);
            parse(consumeToken(), token);
            return typeBoolean;
        }
        case E_FACE_WITH_STUCK_OUT_TONGUE_AND_WINKING_EYE: {
            writer.writeCoin(0x2D);
            
            parse(consumeToken(), token, typeSomeobject);
            parse(consumeToken(), token, typeSomeobject);
            
            return typeBoolean;
        }
        case E_GOAT: {
            if (!initializer) {
                compilerError(token, "🐐 can only be used inside initializers.");
                break;
            }
            if (!contextType.eclass->superclass) {
                compilerError(token, "🐐 can only be used if the eclass inherits from another.");
                break;
            }
            if (calledSuper) {
                compilerError(token, "You may not call more than one superinitializer.");
            }
            if (flowControlDepth) {
                compilerError(token, "You may not put a call to a superinitializer in a flow control structure.");
            }
            
            scoper.topScope()->initializerUnintializedVariablesCheck(token, "Instance variable \"%s\" must be initialized before superinitializer.");
            
            writer.writeCoin(0x3D);
            
            Class *eclass = contextType.eclass;
            
            writer.writeCoin(eclass->superclass->index);
            
            const Token *initializerToken = consumeToken(IDENTIFIER);
            
            Initializer *initializer = eclass->superclass->getInitializer(initializerToken->value[0]);
            
            if (initializer == nullptr) {
                ecCharToCharStack(initializerToken->value[0], initializerString);
                compilerError(initializerToken, "Cannot find superinitializer %s.", initializerString);
                break;
            }
            
            writer.writeCoin(initializer->vti);
            
            checkAccess(initializer, token, "initializer");
            checkArguments(initializer->arguments, contextType, token);
            
            calledSuper = true;
            
            return typeNothingness;
        }
        case E_RED_APPLE: {
            if(effect){
                //effect is true, so apple is called as subcommand
                compilerError(token, "🍎’s return may not be used as an argument.");
            }
            effect = true;
            
            writer.writeCoin(0x60);
            
            if(initializer){
                if (initializer->canReturnNothingness) {
                    parse(consumeToken(), token, typeNothingness);
                    return typeNothingness;
                }
                else {
                    compilerError(token, "🍎 cannot be used inside a initializer.");
                }
            }
            
            parse(consumeToken(), token, callable.returnType);
            returned = true;
            return typeNothingness;
        }
        case E_BLACK_SQUARE_BUTTON: {
            auto placeholder = writer.writeCoinPlaceholder();
            
            Type originalType = parse(consumeToken(), token, typeSomething);
            Type type = Type::parseAndFetchType(contextType, currentNamespace, NoDynamism, nullptr);
            
            if (originalType.compatibleTo(type, contextType)) {
                compilerWarning(token, "Superfluous cast.");
            }
            
            switch (type.type) {
                case TT_CLASS:
                    for (size_t i = 0; i < type.eclass->ownGenericArgumentCount; i++) {
                        if(!type.eclass->genericArgumentContraints[i].compatibleTo(type.genericArguments[i], type) ||
                           !type.genericArguments[i].compatibleTo(type.eclass->genericArgumentContraints[i], type)) {
                            compilerError(token, "Dynamic casts involving generic type arguments are not possible yet. Please specify the generic argument constraints of the class for compatibility with future versions.");
                        }
                    }
                    
                    placeholder.write(originalType.type == TT_SOMETHING || originalType.optional ? 0x44 : 0x40);
                    writer.writeCoin(type.eclass->index);
                    break;
                case TT_PROTOCOL:
                    placeholder.write(originalType.type == TT_SOMETHING || originalType.optional ? 0x45 : 0x41);
                    writer.writeCoin(type.protocol->index);
                    break;
                case TT_BOOLEAN:
                    placeholder.write(0x42);
                    break;
                case TT_INTEGER:
                    placeholder.write(0x43);
                    break;
                case TT_SYMBOL:
                    placeholder.write(0x46);
                    break;
                case TT_DOUBLE:
                    placeholder.write(0x47);
                    break;
                default: {
                    auto typeString = type.toString(contextType, true);
                    compilerError(token, "You cannot cast to %s.", typeString.c_str());
                }
            }
            
            type.optional = true;
            return type;
        }
        case E_BEER_MUG: {
            writer.writeCoin(0x3A);
            
            Type t = parse(consumeToken(), token);
            
            if (!t.optional) {
                compilerError(token, "🍺 can only be used with optionals.");
            }
            
            t.optional = false;
            
            return t;
        }
        case E_CLINKING_BEER_MUGS: {
            writer.writeCoin(0x3B);
            
            auto placeholder = writer.writeCoinsCountPlaceholderCoin();
            
            const Token *methodToken = consumeToken();
            
            Type type = parse(consumeToken(), token);
            if(!type.optional){
                compilerError(token, "🍻 may only be used on 🍬.");
            }
            
            Method *method = type.eclass->getMethod(methodToken->value[0]);
            
            if(method == nullptr){
                auto eclass = type.toString(contextType, true);
                ecCharToCharStack(methodToken->value[0], method);
                compilerError(token, "%s has no method %s", eclass.c_str(), method);
            }
            
            writer.writeCoin(method->vti);
            placeholder.write();
            
            checkAccess(method, token, "method");
            checkArguments(method->arguments, type, token);
            
            Type returnType = method->returnType;
            returnType.optional = true;
            return returnType;
        }
        case E_DOUGHNUT: {
            writer.writeCoin(0x2);
            
            const Token *methodToken = consumeToken(IDENTIFIER);
            
            Type type = Type::parseAndFetchType(contextType, currentNamespace, dynamismLevelFromSI(), nullptr);
            
            if (type.optional) {
                compilerWarning(token, "Please remove useless 🍬.");
            }
            if (type.type != TT_CLASS) {
                compilerError(token, "The given type is not a class.");
            }
            
            writer.writeCoin(type.eclass->index);
            
            ClassMethod *method = type.eclass->getClassMethod(methodToken->value[0]);
            
            if (method == nullptr) {
                auto classString = type.toString(contextType, true);
                ecCharToCharStack(methodToken->value[0], methodString);
                compilerError(token, "%s has no eclass method %s", classString.c_str(), methodString);
            }
            
            writer.writeCoin(method->vti);
            
            checkAccess(method, token, "Class method");
            checkArguments(method->arguments, type, token);
            
            return method->returnType.resolveOn(type);
        }
        case E_HOT_PEPPER: {
            const Token *methodName = consumeToken();
            
            writer.writeCoin(0x71);
            
            Type type = parse(consumeToken(), token);
            
            Method *method;
            if (type.type != TT_CLASS) {
                compilerError(token, "You can only capture method calls on class instances.");
            }
            method = type.eclass->getMethod(methodName->value[0]);
            
            if (!method) {
                compilerError(token, "Method is non-existent.");
            }
            
            writer.writeCoin(method->vti);
            
            return method->type();
        }
        case E_GRAPES: {
            writer.writeCoin(0x70);
            
            auto function = Closure(token);
            function.parseArgumentList(contextType, currentNamespace);
            function.parseReturnType(contextType, currentNamespace);
            
            auto variableCountPlaceholder = writer.writeCoinPlaceholder();
            auto coinCountPlaceholder = writer.writeCoinsCountPlaceholderCoin();
            
            Scope *closingScope = scoper.currentScope();
            if (!inClassContext) {
                scoper.pushScope(scoper.topScope()); //The object scope
            }
            
            function.firstToken = currentToken;
            
            auto sca = StaticFunctionAnalyzer(function, currentNamespace, nullptr, inClassContext, contextType, writer, scoper);
            sca.analyze(true, closingScope);
            
            if (!inClassContext) {
                scoper.popScope();
            }
            
            coinCountPlaceholder.write();
            variableCountPlaceholder.write(sca.localVariableCount());
            writer.writeCoin((EmojicodeCoin)function.arguments.size() | (sca.usedSelfInBody() ? 1 << 16 : 0));
            writer.writeCoin(variableCount);
            
            return function.type();
        }
        case E_LOLLIPOP: {
            writer.writeCoin(0x72);
            
            Type type = parse(consumeToken(), token);
            
            if (type.type != TT_CALLABLE) {
                compilerError(token, "Given value is not callable.");
            }
            
            for (int i = 1; i <= type.arguments; i++) {
                parse(consumeToken(), token, type.genericArguments[i]);
            }
            
            return type.genericArguments[0];
        }
        case E_CHIPMUNK: {
            const Token *nameToken = consumeToken();
            
            if (inClassContext) {
                compilerError(token, "Not within an object-context.");
            }
            
            Class *superclass = contextType.eclass->superclass;
            Method *method = superclass->getMethod(nameToken->value[0]);
            
            if (!method) {
                compilerError(token, "Method is non-existent.");
            }
            
            writer.writeCoin(0x5);
            writer.writeCoin(superclass->index);
            writer.writeCoin(method->vti);
            
            checkArguments(method->arguments, contextType, token);
            
            return method->returnType;
        }
        default: {
            auto placeholder = writer.writeCoinPlaceholder();
            
            const Token *tobject = consumeToken();
            
            Type type = parse(tobject, token);
            
            if (type.optional) {
                compilerError(tobject, "You cannot call methods on optionals.");
            }
            
            Method *method;
            if(type.type == TT_PROTOCOL){
                method = type.protocol->getMethod(token->value[0]);
            }
            else if(type.type == TT_CLASS) {
                method = type.eclass->getMethod(token->value[0]);
            }
            else {
                if(type.type == TT_BOOLEAN){
                    switch (token->value[0]) {
                        case E_NEGATIVE_SQUARED_CROSS_MARK:
                            placeholder.write(0x26);
                            return typeBoolean;
                        case E_PARTY_POPPER:
                            placeholder.write(0x27);
                            parse(consumeToken(), token, typeBoolean);
                            return typeBoolean;
                        case E_CONFETTI_BALL:
                            placeholder.write(0x28);
                            parse(consumeToken(), token, typeBoolean);
                            return typeBoolean;
                    }
                }
                else if (type.type == TT_INTEGER) {
                    switch (token->value[0]) {
                        case E_HEAVY_MINUS_SIGN:
                            placeholder.write(0x21);
                            parse(consumeToken(), token, typeInteger);
                            return typeInteger;
                        case E_HEAVY_PLUS_SIGN:
                            placeholder.write(0x22);
                            parse(consumeToken(), token, typeInteger);
                            return typeInteger;
                        case E_HEAVY_DIVISION_SIGN:
                            placeholder.write(0x24);
                            parse(consumeToken(), token, typeInteger);
                            return typeInteger;
                        case E_HEAVY_MULTIPLICATION_SIGN:
                            placeholder.write(0x23);
                            parse(consumeToken(), token, typeInteger);
                            return typeInteger;
                        case E_LEFT_POINTING_TRIANGLE:
                            placeholder.write(0x29);
                            parse(consumeToken(), token, typeInteger);
                            return typeBoolean;
                        case E_RIGHT_POINTING_TRIANGLE:
                            placeholder.write(0x2A);
                            parse(consumeToken(), token, typeInteger);
                            return typeBoolean;
                        case E_LEFTWARDS_ARROW:
                            placeholder.write(0x2B);
                            parse(consumeToken(), token, typeInteger);
                            return typeBoolean;
                        case E_RIGHTWARDS_ARROW:
                            placeholder.write(0x2C);
                            parse(consumeToken(), token, typeInteger);
                            return typeBoolean;
                        case E_PUT_LITTER_IN_ITS_SPACE:
                            placeholder.write(0x25);
                            parse(consumeToken(), token, typeInteger);
                            return typeInteger;
                    }
                }
                else if (type.type == TT_DOUBLE) {
                    switch (token->value[0]) {
                        case E_FACE_WITH_STUCK_OUT_TONGUE:
                            placeholder.write(0x2F);
                            parse(consumeToken(), token, typeFloat);
                            return typeBoolean;
                        case E_HEAVY_MINUS_SIGN:
                            placeholder.write(0x30);
                            parse(consumeToken(), token, typeFloat);
                            return typeFloat;
                        case E_HEAVY_PLUS_SIGN:
                            placeholder.write(0x31);
                            parse(consumeToken(), token, typeFloat);
                            return typeFloat;
                        case E_HEAVY_DIVISION_SIGN:
                            placeholder.write(0x33);
                            parse(consumeToken(), token, typeFloat);
                            return typeFloat;
                        case E_HEAVY_MULTIPLICATION_SIGN:
                            placeholder.write(0x32);
                            parse(consumeToken(), token, typeFloat);
                            return typeFloat;
                        case E_LEFT_POINTING_TRIANGLE:
                            placeholder.write(0x34);
                            parse(consumeToken(), token, typeFloat);
                            return typeBoolean;
                        case E_RIGHT_POINTING_TRIANGLE:
                            placeholder.write(0x35);
                            parse(consumeToken(), token, typeFloat);
                            return typeBoolean;
                        case E_LEFTWARDS_ARROW:
                            placeholder.write(0x36);
                            parse(consumeToken(), token, typeFloat);
                            return typeBoolean;
                        case E_RIGHTWARDS_ARROW:
                            placeholder.write(0x37);
                            parse(consumeToken(), token, typeFloat);
                            return typeBoolean;
                    }
                }
                
                if(token->value[0] == E_FACE_WITH_STUCK_OUT_TONGUE){
                    parse(consumeToken(), token, type); //Must be of the same type as the callee
                    placeholder.write(0x20);
                    return typeBoolean;
                }
                
                ecCharToCharStack(token->value[0], method);
                auto typeString = type.toString(contextType, true);
                compilerError(token, "Unknown primitive method %s for %s.", method, typeString.c_str());
            }
            
            if(method == nullptr){
                auto eclass = type.toString(contextType, true);
                ecCharToCharStack(token->value[0], method);
                compilerError(token, "%s has no method %s.", eclass.c_str(), method);
            }
            
            if(type.type == TT_PROTOCOL){
                placeholder.write(0x3);
                writer.writeCoin(type.protocol->index);
                writer.writeCoin(method->vti);
            }
            else if(type.type == TT_CLASS) {
                placeholder.write(0x1);
                writer.writeCoin(method->vti);
            }
            
            checkAccess(method, token, "Method");
            checkArguments(method->arguments, type, token);
            
            return method->returnType.resolveOn(type);
        }
    }
    return typeNothingness;
}

StaticFunctionAnalyzer::StaticFunctionAnalyzer(Callable &callable, EmojicodeChar ns, Initializer *i, bool inClassContext, Type contextType, Writer &writer, Scoper &scoper) :
    callable(callable), writer(writer), scoper(scoper), initializer(i), inClassContext(inClassContext), contextType(contextType), currentNamespace(ns) {
    
}

void StaticFunctionAnalyzer::analyze(bool compileDeadCode, Scope *copyScope){
    currentToken = callable.firstToken;
    
    if (initializer) {
        scoper.currentScope()->changeInitializedBy(-1);
    }
    
    //Set the arguments to the method scope
    Scope methodScope(false);
    for (auto variable : callable.arguments) {
        uint8_t id = nextVariableID();
        CompilerVariable *varo = new CompilerVariable(variable.type, id, true, true, callable.dToken);
        
        methodScope.setLocalVariable(variable.name, varo);
    }
    
    if (copyScope) {
        variableCount += methodScope.copyFromScope(copyScope, nextVariableID());
    }
    
    scoper.pushScope(&methodScope);
    
    bool emittedDeadCodeWarning = false;
    
    const Token *token;
    while (token = consumeToken(), !(token->type == IDENTIFIER && token->value[0] == E_WATERMELON)) {
        effect = false;
        
        parse(token, callable.dToken);
        
        noEffectWarning(token);
       
        if(!emittedDeadCodeWarning && returned && nextToken()->value[0] != E_WATERMELON && token->type == IDENTIFIER){
            compilerWarning(consumeToken(), "Dead code.");
            emittedDeadCodeWarning = true;
            if (!compileDeadCode) {
                break;
            }
        }
    }
    
    scoper.currentScope()->recommendFrozenVariables();
    scoper.popScope();
    noReturnError(callable.dToken);
    
    if (initializer) {
        scoper.currentScope()->initializerUnintializedVariablesCheck(initializer->dToken, "Instance variable \"%s\" must be initialized.");
        
        if (!calledSuper && contextType.eclass->superclass) {
            ecCharToCharStack(initializer->name, initializerName);
            compilerError(initializer->dToken, "Missing call to superinitializer in initializer %s.", initializerName);
        }
    }
}

void StaticFunctionAnalyzer::writeAndAnalyzeProcedure(Procedure &procedure, Writer &writer, Type classType, Scoper &scoper, bool inClassContext, Initializer *i) {
    writer.resetWrittenCoins();
    
    writer.writeEmojicodeChar(procedure.name);
    writer.writeUInt16(procedure.vti);
    writer.writeByte((uint8_t)procedure.arguments.size());
    
    if(procedure.native){
        writer.writeByte(1);
        return;
    }
    writer.writeByte(0);
    
    auto variableCountPlaceholder = writer.writePlaceholder<unsigned char>();
    auto coinsCountPlaceholder = writer.writeCoinsCountPlaceholderCoin();
    
    auto sca = StaticFunctionAnalyzer(procedure, procedure.enamespace, i, inClassContext, classType, writer, scoper);
    sca.analyze();
    
    variableCountPlaceholder.write(sca.localVariableCount());
    coinsCountPlaceholder.write();
}