import {
	ScopeObject,
} from "./types";

export let builtinScopeLevel: ScopeObject[] = [];

function addType(name: string) {
	builtinScopeLevel.push({
		type: "alias",
		mutable: false,
		originLocation: "core",
		name: name,
		value: [
			{
				type: "type",
				originLocation: "core",
				name: "core:" + name
			}
		]
	});
}

export function setUpBuiltin() {
	addType("String");
}