import Foundation

class ExternalFunction {
	var hasBeenDefined: Bool
	var LLVM: String
	var data: functionData
	
	init(_ LLVM: String, _ data: functionData) {
		self.hasBeenDefined = false
		self.LLVM = LLVM
		self.data = data
	}
}

protocol hasLocation {
	var lineNumber: Int? { get }
	var start: Int? { get }
	var end: Int? { get }
}

//
// token
//

struct token: hasLocation {
	var name: String
	
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
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

protocol SyntaxProtocol: hasLocation {}

struct SyntaxFunction: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var name: String
	var arguments: [any buildType]
	
	var codeBlock: [any SyntaxProtocol]
	
	var returnType: any buildType
}

struct SyntaxInclude: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var names: [any SyntaxProtocol]
}

struct SyntaxReturn: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var value: any SyntaxProtocol
}

//struct SyntaxType: SyntaxProtocol {
//	var value: any SyntaxProtocol
//}

struct SyntaxSimpleType: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var value: String
}

struct SyntaxWord: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var value: String
}

struct SyntaxString: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var value: String
}

struct SyntaxNumber: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var value: String
}

struct SyntaxCall: SyntaxProtocol {
	var lineNumber: Int?
	var start: Int?
	var end: Int?
	
	var name: String
	var arguments: [any SyntaxProtocol]
}

//
// build variables
//

protocol buildVariable: AnyObject {}

class functionData: buildVariable {
	var name: String
	var LLVMname: String
	var arguments: [any buildType] = []
	var returnType: any buildType = buildTypeSimple("null")
	
	var instructionCount: Int = 0
	
	var hasReturned: Bool = false
	
	var LLVMString: String = ""
	
	init(_ name: String, _ LLVMname: String, _ arguments: [any buildType], _ returnType: any buildType) {
		self.name = name
		self.LLVMname = LLVMname
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
