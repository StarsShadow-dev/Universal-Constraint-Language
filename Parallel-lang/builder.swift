import Foundation

func buildLLVM(_ builderContext: BuilderContext, _ inside: functionData?, _ AST: [any SyntaxProtocol], _ typeList: [any buildType]?, _ newLevel: Bool) -> String {
	
	if let typeList {
		if (AST.count > typeList.count) {
			compileError("too many arguments")
		} else if (AST.count < typeList.count) {
			compileError("not enough arguments")
		}
	}
	
	var LLVMSource: String = ""
	
	var index = 0
	
	if (newLevel) {
		builderContext.level += 1
		builderContext.variables.append([:])
	}
	
	while (true) {
		if (index >= AST.count) {
			break
		}

		let node = AST[index]

		if let node = node as? SyntaxFunction {
			let data = functionData(node.name, node.name, node.arguments, node.returnType)
			
			if let returnType = node.returnType as? buildTypeSimple {
				if (returnType.name != "Int32") {
					compileErrorWithHasLocation("functions can only return Int32 right now", node)
				}
			} else {
				abort()
			}
			
			builderContext.variables[builderContext.level][node.name] = data
		}
		
		else if let node = node as? SyntaxInclude {
			for name in node.names {
				if let name = name as? SyntaxWord {
					if let externalFunction = externalFunctions[name.value] {
						if (externalFunction.hasBeenDefined) {
							compileErrorWithHasLocation("external function \(name.value) has already been defined", name)
						}
						
						builderContext.variables[0][name.value] = externalFunction.data
						
						toplevelLLVM += externalFunction.LLVM + "\n"
						externalFunction.hasBeenDefined = true
					} else {
						compileErrorWithHasLocation("external function \(name.value) does not exist", name)
					}
				} else {
					compileErrorWithHasLocation("not a word in an include statement", name)
				}
			}
		}
		
		else {
			if (builderContext.level == 0) {
				compileErrorWithHasLocation("only functions and include are allowed at top level", node)
			}
		}

		index += 1
	}
	
	index = 0
	
	while (true) {
		if (index >= AST.count) {
			break
		}
		
		let node = AST[index]
		
		if let node = node as? SyntaxFunction {
			if let function = builderContext.variables[builderContext.level][node.name] as? functionData {
				let build = buildLLVM(builderContext, function, node.codeBlock, nil, true)
				
				if (!function.hasReturned) {
					compileErrorWithHasLocation("function `\(node.name)` never returned", node)
				}
				
				LLVMSource.append("\ndefine i32 @\(node.name)() {\nentry:")
				
				LLVMSource.append(function.LLVMString)
				
				LLVMSource.append("\n}")
			} else {
				abort()
			}
		}
		
		else if let _ = node as? SyntaxInclude {
			
		}
		
		else if let node = node as? SyntaxReturn {
			guard let inside else {
				compileErrorWithHasLocation("return statement is outside of a function", node)
			}
			
			inside.LLVMString.append("\n\tret \(buildLLVM(builderContext, inside, [node.value], [inside.returnType], false))")
			
			(getVariable(inside.name) as! functionData).hasReturned = true
		}
		
		else if let node = node as? SyntaxCall {
			guard let inside else {
				compileErrorWithHasLocation("call outside of a function", node)
			}
			
			guard let function = getVariable(inside.name) as? functionData else {
				abort()
			}
			
			guard let functionToCall = getVariable(node.name) as? functionData else {
				compileErrorWithHasLocation("call to unknown function \"\(node.name)\"", node)
			}
			
			let string: String = buildLLVM(builderContext, inside, node.arguments, functionToCall.arguments, false)
			
			function.LLVMString.append("\n\t%\(function.instructionCount) = call i32 @\(node.name)(\(string))")
			
			#warning("i32 should not be hard coded")
			LLVMSource.append("i32 %\(function.instructionCount)")
			
			function.instructionCount += 1
		}
		
		else if let node = node as? SyntaxNumber {
			if let type = typeList![index] as? buildTypeSimple {
				if (type.name == "Int32") {
					LLVMSource.append("i32 \(node.value)")
				} else {
					compileErrorWithHasLocation("expected type Int32 but got type \(type.name)", node)
				}
			} else {
				compileErrorWithHasLocation("not buildTypeSimple?", node)
			}
		}
		
		else {
			compileErrorWithHasLocation("unexpected node type \(node)", node)
		}
		
		index += 1
	}
	
	if (newLevel) {
		builderContext.level -= 1
		let _ = builderContext.variables.popLast()
	}
	
	return LLVMSource
	
	func getVariable(_ name: String) -> (any buildVariable)? {
		var i = builderContext.variables.count - 1
		while i >= 0 {
			if (builderContext.variables[i][name] != nil) {
				return builderContext.variables[i][name]!
			}
			
			i -= 1
		}
		
		return nil
	}
}
