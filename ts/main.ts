import { setUpBuiltin } from "./builtin";
import { compileFile } from "./compiler";

setUpBuiltin();

try {
	compileFile("test/main.llcl");
} catch (error) {
	if (error == "__do nothing__") {
		// do nothing
	} else {
		throw error;
	}
}