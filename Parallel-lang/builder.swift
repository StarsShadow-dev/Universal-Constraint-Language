import Foundation

func buildLLVM(_ builderContext: BuilderContext, _ insideFunction: functionData?, _ insideNode: SyntaxProtocol?, _ AST: [any SyntaxProtocol], _ typeList: [any buildType]?, _ newLevel: Bool) -> String {
	
	if let typeList {
		if (AST.count > typeList.count) {
			compileErrorWithHasLocation("too many arguments expected \(typeList.count) but got \(AST.count)", insideNode!)
		} else if (AST.count < typeList.count) {
			compileErrorWithHasLocation("not enough arguments expected \(typeList.count) but got \(AST.count)", insideNode!)
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
				let _ = buildLLVM(builderContext, function, node, node.codeBlock, nil, true)
				
				if (!function.hasReturned) {
					compileErrorWithHasLocation("function `\(node.name)` never returned", node)
				}
				
				if let returnType = function.returnType as? buildTypeSimple {
					guard let LLVMType = LLVMTypeMap[returnType.name] else {
						compileErrorWithHasLocation("no type called \(returnType.name)", node)
					}
					
					// if the function is not named main make it private
					// https://llvm.org/docs/LangRef.html#linkage-types
					if (node.name == "main") {
						LLVMSource.append("\ndefine \(LLVMType.0) @\(node.name)() {\nentry:")
					} else {
						LLVMSource.append("\ndefine private \(LLVMType.0) @\(node.name)() {\nentry:")
					}
					
					LLVMSource.append(function.LLVMString)
					
					LLVMSource.append("\n}")
				} else {
					abort()
				}
			} else {
				abort()
			}
		}
		
		else if let _ = node as? SyntaxInclude {
			
		}
		
		else if let node = node as? SyntaxReturn {
			guard let insideFunction else {
				compileErrorWithHasLocation("return statement is outside of a function", node)
			}
			
			insideFunction.LLVMString.append("\n\tret \(buildLLVM(builderContext, insideFunction, node, [node.value], [insideFunction.returnType], false))")
			
			(getVariable(insideFunction.name) as! functionData).hasReturned = true
		}
		
		else if let node = node as? SyntaxDefinition {
			guard let insideFunction else {
				compileErrorWithHasLocation("variable definition outside of a function", node)
			}
			
			if let variableType = node.type as? buildTypeSimple {
				guard let LLVMType = LLVMTypeMap[variableType.name] else {
					compileErrorWithHasLocation("no type called \(variableType.name)", node)
				}
				
				if (getVariable(node.name) != nil) {
					compileErrorWithHasLocation("Invalid redeclaration of '\(node.name)'", node)
				}
				
				builderContext.variables[builderContext.level][node.name] = variableData(node.type, insideFunction.instructionCount, false)
				
				insideFunction.LLVMString.append("\n\t%\(insideFunction.instructionCount) = alloca \(LLVMType.0)")
				
				insideFunction.instructionCount += 1
			} else {
				abort()
			}
		}
		
		else if let node = node as? SyntaxAssignment {
			guard let insideFunction else {
				compileErrorWithHasLocation("variable assignment outside of a function", node)
			}
			
			guard let variable = getVariable(node.name) else {
				compileErrorWithHasLocation("variable not found", node)
			}
			
			if let variable = variable as? variableData {
				variable.initialized = true
				insideFunction.LLVMString.append("\n\tstore \(buildLLVM(builderContext, insideFunction, node, node.expression, [insideFunction.returnType], false)), ptr %\(variable.index)")
			} else {
				abort()
			}
		}
		
		else if let node = node as? SyntaxCall {
			guard let insideFunction else {
				compileErrorWithHasLocation("call outside of a function", node)
			}
			
			guard let function = getVariable(insideFunction.name) as? functionData else {
				abort()
			}
			
			guard let functionToCall = getVariable(node.name) as? functionData else {
				compileErrorWithHasLocation("call to unknown function \"\(node.name)\"", node)
			}
			
			if let returnType = functionToCall.returnType as? buildTypeSimple {
				guard let LLVMType = LLVMTypeMap[returnType.name] else {
					compileErrorWithHasLocation("no type called \(returnType.name)", node)
				}
				
				let argumentsString: String = buildLLVM(builderContext, insideFunction, node, node.arguments, functionToCall.arguments, false)
				
				if (LLVMType.0 == "void") {
					function.LLVMString.append("\n\tcall \(LLVMType.0) @\(node.name)(\(argumentsString))")
				} else {
					function.LLVMString.append("\n\t%\(function.instructionCount) = call \(LLVMType.0) @\(node.name)(\(argumentsString))")
					
					LLVMSource.append("\(LLVMType.0) %\(function.instructionCount)")
					
					function.instructionCount += 1
				}
			} else {
				abort()
			}
		}
		
		else if let node = node as? SyntaxWord {
			if (node.value == "Void") {
				guard let type = typeList![index] as? buildTypeSimple else {
					compileErrorWithHasLocation("not buildTypeSimple?", node)
				}
				
				if (type.name == "Void") {
					LLVMSource.append("void")
				} else {
					compileErrorWithHasLocation("expected type `\(type.name)` but got type `Void`", node)
				}
			} else {
				guard let insideFunction else {
					compileErrorWithHasLocation("variable outside of a function", node)
				}
				
				guard let variable = getVariable(node.value) else {
					compileErrorWithHasLocation("variable not found", node)
				}
				
				if let variable = variable as? variableData {
					if let variableType = variable.type as? buildTypeSimple {
						guard let LLVMType = LLVMTypeMap[variableType.name] else {
							abort()
						}
						
						if (!variable.initialized) {
							compileErrorWithHasLocation("variable '\(node.value)' has not been initialized", node)
						}
						
						insideFunction.LLVMString.append("\n\t%\(insideFunction.instructionCount) = load \(LLVMType.0), ptr %\(variable.index)")
						
						LLVMSource.append("\(LLVMType.0) %\(insideFunction.instructionCount)")
						
						insideFunction.instructionCount += 1
					} else {
						abort()
					}
				} else {
					abort()
				}
			}
		}
		
		else if let node = node as? SyntaxNumber {
			if let type = typeList![index] as? buildTypeSimple {
				if (type.name == "Int32") {
					LLVMSource.append("i32 \(node.value)")
				} else {
					compileErrorWithHasLocation("expected type `\(type.name)` but got type `Int32`", node)
				}
			} else {
				compileErrorWithHasLocation("not buildTypeSimple?", node)
			}
		}
		
		else if let node = node as? SyntaxString {
			let index = constants.lastIndex(of: node.value)
			
			if (index != nil) {
				LLVMSource.append("ptr @.const.\(String(describing: index))")
			} else {
				// if the constant does exist yet
				LLVMSource.append("ptr @.const.\(constants.count)")
				constants.append(node.value)
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
