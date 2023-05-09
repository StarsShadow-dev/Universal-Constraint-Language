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
// build
//

public protocol buildVariable: AnyObject {}

public class functionData: buildVariable {
//	var name: String
	var arguments: [any SyntaxProtocol] = []
	
	var instructionCount: Int = 0
	
	var hasReturned: Bool = false
}

public class variableData: buildVariable {
//	var name: String
	var type: String = ""
}

//
// AST
//

public protocol SyntaxProtocol {}

public struct SyntaxFunction: SyntaxProtocol {
	var name: String
	var arguments: [any SyntaxProtocol]
	
	var codeBlock: [any SyntaxProtocol]
	
	var returnValue: any buildVariable
}

public struct SyntaxInclude: SyntaxProtocol {
	var names: [any SyntaxProtocol]
}

public struct SyntaxReturn: SyntaxProtocol {
	var value: any SyntaxProtocol
}

//public struct SyntaxType: SyntaxProtocol {
//	var value: any SyntaxProtocol
//}

public struct SyntaxSimpleType: SyntaxProtocol {
	var value: String
}

public struct SyntaxWord: SyntaxProtocol {
	var value: String
}

public struct SyntaxString: SyntaxProtocol {
	var value: String
}

public struct SyntaxNumber: SyntaxProtocol {
	var value: String
}

public struct SyntaxCall: SyntaxProtocol {
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
