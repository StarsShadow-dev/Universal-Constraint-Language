// import { ASTnode } from "./parser";
// import { SourceLocation } from "./types";
// import utilities from "./utilities";

// type genericIRNode = {
// 	location: SourceLocation,
// };

// export type IRNode = 
// genericIRNode & {
// 	kind: "bool",
// 	value: boolean,
// } |
// genericIRNode & {
// 	kind: "number",
// 	value: number,
// } |
// genericIRNode & {
// 	kind: "string",
// 	value: string,
// } |
// genericIRNode & {
// 	kind: "identifier",
// 	name: string,
// } |
// genericIRNode & {
// 	kind: "call",
// 	left: IRNode,
// 	callArguments: IRNode[],
// };

// export function ASTtoIR(ASTnode: ASTnode): IRNode {
// 	switch (ASTnode.kind) {
// 		case "bool": {
// 			return {
// 				kind: "bool",
// 				location: ASTnode.location,
// 				value: ASTnode.value,
// 			};
// 		}
// 		case "number": {
// 			return {
// 				kind: "number",
// 				location: ASTnode.location,
// 				value: ASTnode.value,
// 			};
// 		}
// 		case "string": {
// 			return {
// 				kind: "string",
// 				location: ASTnode.location,
// 				value: ASTnode.value,
// 			};
// 		}
		
// 		case "identifier": {
// 			return {
// 				kind: "identifier",
// 				location: ASTnode.location,
// 				name: ASTnode.name,
// 			};
// 		}
		
// 		case "call": {
// 			return {
// 				kind: "call",
// 				location: ASTnode.location,
// 				left: ASTtoIR(ASTnode.left),
// 				callArguments: getIR(ASTnode.callArguments),
// 			};
// 		}
	
// 		default: {
// 			throw utilities.TODO();
// 		}
// 	}
// }

// export function getIR(AST: ASTnode[]): IRNode[] {
// 	let IRNodes: IRNode[] = [];
// 	for (let i = 0; i < AST.length; i++) {
// 		IRNodes.push(ASTtoIR(AST[i]));
// 	}
// 	return IRNodes;
// }