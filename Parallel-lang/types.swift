import Foundation

//
// token
//

struct token {
	var name: String
	
	var lineNumber: Int
	var start: Int
	var end: Int
	
	var value: String
}

//
// context
//

struct ParserContext {
	var index: Int
}

class BuilderContext {
	var level: Int
	
	var variables: [[String:any buildVariable]]
	
	init(level: Int, variables: [[String : any buildVariable]]) {
		self.level = level
		self.variables = variables
	}
}

//
// AST
//

protocol SyntaxProtocol {}

struct SyntaxFunction: SyntaxProtocol {
	var name: String
	var arguments: [any SyntaxProtocol]
	
	var codeBlock: [any SyntaxProtocol]
	
	var returnType: any buildType
}

struct SyntaxInclude: SyntaxProtocol {
	var names: [any SyntaxProtocol]
}

struct SyntaxReturn: SyntaxProtocol {
	var value: any SyntaxProtocol
}

//struct SyntaxType: SyntaxProtocol {
//	var value: any SyntaxProtocol
//}

struct SyntaxSimpleType: SyntaxProtocol {
	var value: String
}

struct SyntaxWord: SyntaxProtocol {
	var value: String
}

struct SyntaxString: SyntaxProtocol {
	var value: String
}

struct SyntaxNumber: SyntaxProtocol {
	var value: String
}

struct SyntaxCall: SyntaxProtocol {
	var name: String
	var arguments: [any SyntaxProtocol]
}


class ExternalFunction {
	var hasBeenDefined: Bool
	var LLVM: String
	
	init(_ LLVM: String) {
		self.hasBeenDefined = false
		self.LLVM = LLVM
	}
}

//
// build variables
//

protocol buildVariable: AnyObject {}

class functionData: buildVariable {
//	var name: String
	var arguments: [any SyntaxProtocol] = []
	var returnType: any buildType = buildTypeSimple("null")
	
	var instructionCount: Int = 0
	
	var hasReturned: Bool = false
	
	init(_ arguments: [any SyntaxProtocol], _ returnType: any buildType) {
		self.arguments = arguments
		self.returnType = returnType
	}
}

class variableData: buildVariable {
//	var name: String
	var type: String = ""
}

//
// build types
//

protocol buildType: AnyObject {}

// a simple type that only contains a name and a value
class buildTypeSimple: buildType {
	var name: String
	var value: String = ""
	
	init(_ name: String) {
		self.name = name
	}
}
