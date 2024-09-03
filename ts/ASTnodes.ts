import * as utilities from "./utilities.js";
import { BuilderContext } from "./db.js";
import { SourceLocation } from "./types.js";
import { CompileError } from "./report.js";
import { getBuiltinType } from "./builtin.js";

export class CodeGenContext {
	level = 0;
};

export class ASTnode {
	constructor(
		public location: SourceLocation,
	) {}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		utilities.TODO();
	};
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		utilities.TODO();
	};
	
	evaluate(context: BuilderContext): ASTnode {
		utilities.TODO();
	}
}

//#region literals

export class ASTnode_bool extends ASTnode {
	constructor(
		location: SourceLocation,
		public value: boolean,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		if (this.value) {
			return "true";
		} else {
			return "false";
		}
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("Bool");
	}
}
export class ASTnode_number extends ASTnode {
	constructor(
		location: SourceLocation,
		public value: number,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		return `${this.value}`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("Number");
	}
}
export class ASTnode_string extends ASTnode {
	constructor(
		location: SourceLocation,
		public value: string,
	) {
		super(location);
	}
	
	print(context: CodeGenContext = new CodeGenContext()): string {
		return `"${this.value}"`;
	}
	
	getType(context: BuilderContext): ASTnodeType | ASTnode_error {
		return getBuiltinType("String");
	}
}
export class ASTnode_list extends ASTnode {
	constructor(
		location: SourceLocation,
		public elements: ASTnode[],
	) {
		super(location);
	}
};

//#endregion

//#region types

export abstract class ASTnodeType extends ASTnode {
	constructor(
		location: SourceLocation,
		public id: string,
	) {
		super(location);
	}
}

export class ASTnodeType_struct extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public fields: ASTnode_argument[],
		public data?: {
			kind: "number",
			min: number,
			max: number,
		},
	) {
		super(location, id);
	}
}

export class ASTnodeType_enum extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public codeBlock: ASTnode[],
	) {
		super(location, id);
	}
}

export class ASTnodeType_functionType extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public arg: ASTnode,
		public returnType: ASTnode,
	) {
		super(location, id);
	}
}

export class ASTnodeType_selfType extends ASTnodeType {
	constructor(
		location: SourceLocation,
		id: string,
		public type: ASTnodeType,
	) {
		super(location, id);
	}
}

//#endregion

//#region other

export class ASTnode_function extends ASTnode {
	hasError = false;
	
	constructor(
		location: SourceLocation,
		public arg: ASTnode_argument,
		public body: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_argument extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public type: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_alias extends ASTnode {
	unalias = false;
	
	constructor(
		location: SourceLocation,
		public left: ASTnode,
		public value: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_identifier extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
	) {
		super(location);
	}
};

export class ASTnode_call extends ASTnode {
	constructor(
		location: SourceLocation,
		public left: ASTnode,
		public arg: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_operator extends ASTnode {
	constructor(
		location: SourceLocation,
		public operatorText: string,
		public left: ASTnode,
		public right: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_field extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public type: ASTnode,
	) {
		super(location);
	}
};

export class ASTnode_match extends ASTnode {
	constructor(
		location: SourceLocation,
		public expression: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_matchCase extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public types: ASTnode[],
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_while extends ASTnode {
	constructor(
		location: SourceLocation,
		public condition: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_if extends ASTnode {
	constructor(
		location: SourceLocation,
		public condition: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_codeBlock extends ASTnode {
	constructor(
		location: SourceLocation,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

export class ASTnode_instance extends ASTnode {
	constructor(
		location: SourceLocation,
		public template: ASTnode,
		public codeBlock: ASTnode[],
	) {
		super(location);
	}
};

//#endregion

//#region internal

export class ASTnode_error extends ASTnode {
	constructor(
		location: SourceLocation,
		public name: string,
		public compileError: CompileError | null,
	) {
		super(location);
	}
};

export class ASTnode_unknowable extends ASTnode {
	constructor(
		location: SourceLocation,
	) {
		super(location);
	}
};

//#endregion

//#region utilities

export function evaluateList(context: BuilderContext, AST: ASTnode[]): ASTnode[] {
	let aliasCount = 0;
	
	let output = [];
	for (let i = 0; i < AST.length; i++) {
		const ASTnode = AST[i];
		const result = ASTnode.evaluate(context);
		if (result instanceof ASTnode_alias) {
			if (context.setUnalias) {
				result.unalias = true;
			}
			// context.aliases.push(makeAlias(result.location, false, result.name, result));
			context.aliases.push(result);
			aliasCount++;
		}
		// if (ASTnode.kind == "identifier") {
		// 	const alias = getAlias(context, ASTnode.name);
		// 	if (alias) {
		// 		if (alias.unalias) {
		// 			logger.log(LogType.evaluate, `unalias ${ASTnode.name}`);
		// 			output.push(AST[i]);
		// 			continue;
		// 		} else {
		// 			logger.log(LogType.evaluate, `no unalias ${ASTnode.name}`);
		// 		}
		// 	}
		// }
		output.push(result);
	}
	
	for (let i = 0; i < aliasCount; i++) {
		context.aliases.pop();
	}
	
	return output;
}

//#endregion