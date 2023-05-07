import Foundation

public protocol buildVariable {}

public struct functionData: buildVariable {
//	var name: String
	var arguments: [any SyntaxProtocol]
	
	var codeBlock: [any SyntaxProtocol]
}

public struct variableData: buildVariable {
//	var name: String
	var type: String
}

public protocol SyntaxProtocol {}

public struct SyntaxFunction: SyntaxProtocol {
	var name: String
	var arguments: [any SyntaxProtocol]
	
	var codeBlock: [any SyntaxProtocol]
	
	var returnValue: any buildVariable
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
