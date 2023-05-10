import Foundation

func buildLLVM(_ builderContext: BuilderContext, _ inside: (any buildVariable)?, _ AST: [any SyntaxProtocol], _ typeList: [any buildType]?, _ newLevel: Bool) -> String {
	
	if let typeList {
		if (AST.count > typeList.count) {
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
			let data = functionData(node.arguments, node.returnType)
			
			builderContext.variables[builderContext.level][node.name] = data
		}
		
		else if let node = node as? SyntaxInclude {
			for name in node.names {
				if let name = name as? SyntaxWord {
					if let externalFunction = externalFunctions[name.value] {
						if (externalFunction.hasBeenDefined) {
							compileErrorWithHasLocation("external function \(name.value) has already been defined", name)
						}
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
			let function = builderContext.variables[builderContext.level][node.name]
			let build = buildLLVM(builderContext, function, node.codeBlock, nil, true)
			
			LLVMSource.append("\ndefine i32 @\(node.name)() {\nentry:")
			
			LLVMSource.append(build)
			
			LLVMSource.append("\n}")
		}
		
		else if let _ = node as? SyntaxInclude {
			
		}
		
		else if let node = node as? SyntaxReturn {
			if let inside = inside as? functionData {
				LLVMSource.append("\n\tret \(buildLLVM(builderContext, inside, [node.value], [inside.returnType], false))")
			} else {
				compileErrorWithHasLocation("return outside of a function", node)
			}
		}
		
		else if let node = node as? SyntaxCall {
			if let inside = inside as? functionData {
				let string: String = buildLLVM(builderContext, inside, node.arguments, [], false)
				
				LLVMSource.append("\n\t%\(inside.instructionCount) = call i32 @\(node.name)(\(string))")
				inside.instructionCount += 1
			} else {
				compileErrorWithHasLocation("call outside of a function", node)
			}
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
}
