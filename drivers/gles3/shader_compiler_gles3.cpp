#include "shader_compiler_gles3.h"
#include "os/os.h"

#define SL ShaderLanguage

static String _mktab(int p_level) {

	String tb;
	for(int i=0;i<p_level;i++) {
		tb+="\t";
	}

	return tb;
}

static String _typestr(SL::DataType p_type) {

	return ShaderLanguage::get_datatype_name(p_type);
}

static int _get_datatype_size(SL::DataType p_type) {

	switch(p_type) {

		case SL::TYPE_VOID: return 0;
		case SL::TYPE_BOOL: return 4;
		case SL::TYPE_BVEC2: return 8;
		case SL::TYPE_BVEC3: return 16;
		case SL::TYPE_BVEC4: return 16;
		case SL::TYPE_INT: return 4;
		case SL::TYPE_IVEC2: return 8;
		case SL::TYPE_IVEC3: return 16;
		case SL::TYPE_IVEC4: return 16;
		case SL::TYPE_UINT: return 4;
		case SL::TYPE_UVEC2: return 8;
		case SL::TYPE_UVEC3: return 16;
		case SL::TYPE_UVEC4: return 16;
		case SL::TYPE_FLOAT: return 4;
		case SL::TYPE_VEC2: return 8;
		case SL::TYPE_VEC3: return 16;
		case SL::TYPE_VEC4: return 16;
		case SL::TYPE_MAT2: return 16;
		case SL::TYPE_MAT3: return 48;
		case SL::TYPE_MAT4: return 64;
		case SL::TYPE_SAMPLER2D: return 16;
		case SL::TYPE_ISAMPLER2D: return 16;
		case SL::TYPE_USAMPLER2D: return 16;
		case SL::TYPE_SAMPLERCUBE: return 16;
	}


}


static String _prestr(SL::DataPrecision p_pres) {


	switch(p_pres) {
		case SL::PRECISION_LOWP: return "lowp ";
		case SL::PRECISION_MEDIUMP: return "mediump ";
		case SL::PRECISION_HIGHP: return "highp ";
		case SL::PRECISION_DEFAULT: return "";
	}
	return "";
}


static String _opstr(SL::Operator p_op) {

	return SL::get_operator_text(p_op);
}

static String _mkid(const String& p_id) {

	return "m_"+p_id;
}

static String get_constant_text(SL::DataType p_type, const Vector<SL::ConstantNode::Value>& p_values) {

	switch(p_type) {
		case SL::TYPE_BOOL:  return p_values[0].boolean?"true":"false";
		case SL::TYPE_BVEC2:  return String()+"bvec2("+(p_values[0].boolean?"true":"false")+(p_values[1].boolean?"true":"false")+")";
		case SL::TYPE_BVEC3:  return String()+"bvec3("+(p_values[0].boolean?"true":"false")+","+(p_values[1].boolean?"true":"false")+","+(p_values[2].boolean?"true":"false")+")";
		case SL::TYPE_BVEC4:  return String()+"bvec4("+(p_values[0].boolean?"true":"false")+","+(p_values[1].boolean?"true":"false")+","+(p_values[2].boolean?"true":"false")+","+(p_values[3].boolean?"true":"false")+")";
		case SL::TYPE_INT:  return rtos(p_values[0].sint);
		case SL::TYPE_IVEC2:  return String()+"ivec2("+rtos(p_values[0].sint)+","+rtos(p_values[1].sint)+")";
		case SL::TYPE_IVEC3:  return String()+"ivec3("+rtos(p_values[0].sint)+","+rtos(p_values[1].sint)+","+rtos(p_values[2].sint)+")";
		case SL::TYPE_IVEC4:  return String()+"ivec4("+rtos(p_values[0].sint)+","+rtos(p_values[1].sint)+","+rtos(p_values[2].sint)+","+rtos(p_values[3].sint)+")";
		case SL::TYPE_UINT:  return rtos(p_values[0].real);
		case SL::TYPE_UVEC2:  return String()+"uvec2("+rtos(p_values[0].real)+","+rtos(p_values[1].real)+")";
		case SL::TYPE_UVEC3:  return String()+"uvec3("+rtos(p_values[0].real)+","+rtos(p_values[1].real)+","+rtos(p_values[2].real)+")";
		case SL::TYPE_UVEC4:  return String()+"uvec4("+rtos(p_values[0].real)+","+rtos(p_values[1].real)+","+rtos(p_values[2].real)+","+rtos(p_values[3].real)+")";
		case SL::TYPE_FLOAT:  return rtos(p_values[0].real);
		case SL::TYPE_VEC2:  return String()+"vec2("+rtos(p_values[0].real)+","+rtos(p_values[1].real)+")";
		case SL::TYPE_VEC3:  return String()+"vec3("+rtos(p_values[0].real)+","+rtos(p_values[1].real)+","+rtos(p_values[2].real)+")";
		case SL::TYPE_VEC4:  return String()+"vec4("+rtos(p_values[0].real)+","+rtos(p_values[1].real)+","+rtos(p_values[2].real)+","+rtos(p_values[3].real)+")";
		default: ERR_FAIL_V(String());
	}
}

void ShaderCompilerGLES3::_dump_function_deps(SL::ShaderNode* p_node, const StringName& p_for_func, const Map<StringName,String>& p_func_code, String& r_to_add, Set<StringName> &added) {

	int fidx=-1;

	for(int i=0;i<p_node->functions.size();i++) {
		if (p_node->functions[i].name==p_for_func) {
			fidx=i;
			break;
		}
	}

	ERR_FAIL_COND(fidx==-1);

	for (Set<StringName>::Element *E=p_node->functions[fidx].uses_function.front();E;E=E->next()) {

		if (added.has(E->get())) {
			continue; //was added already
		}

		_dump_function_deps(p_node,E->get(),p_func_code,r_to_add,added);

		SL::FunctionNode *fnode=NULL;

		for(int i=0;i<p_node->functions.size();i++) {
			if (p_node->functions[i].name==E->get()) {
				fnode=p_node->functions[i].function;
				break;
			}
		}

		ERR_FAIL_COND(!fnode);

		r_to_add+="\n";

		String header;
		header=_typestr(fnode->return_type)+" "+_mkid(fnode->name)+"(";
		for(int i=0;i<fnode->arguments.size();i++) {

			if (i>0)
				header+=", ";
			header+=_prestr(fnode->arguments[i].precision)+_typestr(fnode->arguments[i].type)+" "+_mkid(fnode->arguments[i].name);
		}

		header+=")\n";
		r_to_add+=header;
		r_to_add+=p_func_code[E->get()];

		added.insert(E->get());
	}
}

String ShaderCompilerGLES3::_dump_node_code(SL::Node *p_node, int p_level, GeneratedCode& r_gen_code, IdentifierActions &p_actions, const DefaultIdentifierActions &p_default_actions) {

	String code;

	switch(p_node->type) {

		case SL::Node::TYPE_SHADER: {

			SL::ShaderNode *pnode=(SL::ShaderNode*)p_node;

			for(int i=0;i<pnode->render_modes.size();i++) {

				if (p_default_actions.render_mode_defines.has(pnode->render_modes[i]) && !used_rmode_defines.has(pnode->render_modes[i])) {

					r_gen_code.defines.push_back(p_default_actions.render_mode_defines[pnode->render_modes[i]].utf8());
					used_rmode_defines.insert(pnode->render_modes[i]);
				}

				if (p_actions.render_mode_flags.has(pnode->render_modes[i])) {
					*p_actions.render_mode_flags[pnode->render_modes[i]]=true;
				}

				if (p_actions.render_mode_values.has(pnode->render_modes[i])) {
					Pair<int*,int> &p = p_actions.render_mode_values[pnode->render_modes[i]];
					*p.first=p.second;
				}
			}


			int max_texture_uniforms=0;
			int max_uniforms=0;

			for(Map<StringName,SL::ShaderNode::Uniform>::Element *E=pnode->uniforms.front();E;E=E->next()) {
				if (SL::is_sampler_type(E->get().type))
					max_texture_uniforms++;
				else
					max_uniforms++;
			}

			r_gen_code.texture_uniforms.resize(max_texture_uniforms);

			Vector<int> uniform_sizes;
			uniform_sizes.resize(max_uniforms);

			for(Map<StringName,SL::ShaderNode::Uniform>::Element *E=pnode->uniforms.front();E;E=E->next()) {

				String ucode="uniform ";
				ucode+=_prestr(E->get().precission);
				ucode+=_typestr(E->get().type);
				ucode+=" "+_mkid(E->key());
				ucode+=";\n";
				if (SL::is_sampler_type(E->get().type)) {
					r_gen_code.vertex_global+=ucode;
					r_gen_code.fragment_global+=ucode;
					r_gen_code.texture_uniforms[E->get().texture_order]=_mkid(E->key());
				} else {
					if (r_gen_code.uniforms.empty()) {

						r_gen_code.defines.push_back(String("#define USE_MATERIAL\n").ascii());
					}
					r_gen_code.uniforms+=ucode;
					uniform_sizes[E->get().order]=_get_datatype_size(E->get().type);
				}

				p_actions.uniforms->insert(E->key(),E->get());

			}

			// add up
			for(int i=0;i<uniform_sizes.size();i++) {

				if (i>0)
					uniform_sizes[i]=uniform_sizes[i]+uniform_sizes[i-1];
			}
			//offset
			r_gen_code.uniform_offsets.resize(uniform_sizes.size());
			for(int i=0;i<uniform_sizes.size();i++) {

				if (i>0)
					r_gen_code.uniform_offsets[i]=uniform_sizes[i]-1;
				else
					r_gen_code.uniform_offsets[i]=0;
			}

			if (uniform_sizes.size()) {
				r_gen_code.uniform_total_size=uniform_sizes[ uniform_sizes.size() -1 ];
			} else {
				r_gen_code.uniform_total_size=0;
			}

			for(Map<StringName,SL::ShaderNode::Varying>::Element *E=pnode->varyings.front();E;E=E->next()) {

				String vcode;
				vcode+=_prestr(E->get().precission);
				vcode+=_typestr(E->get().type);
				vcode+=" "+String(E->key());
				vcode+=";\n";
				r_gen_code.vertex_global+="out "+vcode;
				r_gen_code.fragment_global+="in "+vcode;
			}

			Map<StringName,String> function_code;

			//code for functions
			for(int i=0;i<pnode->functions.size();i++) {
				SL::FunctionNode *fnode=pnode->functions[i].function;
				function_code[fnode->name]=_dump_node_code(fnode->body,p_level+1,r_gen_code,p_actions,p_default_actions);
			}

			//place functions in actual code

			Set<StringName> added_vtx;
			Set<StringName> added_fragment; //share for light

			for(int i=0;i<pnode->functions.size();i++) {

				SL::FunctionNode *fnode=pnode->functions[i].function;


				if (fnode->name=="vertex") {

					_dump_function_deps(pnode,fnode->name,function_code,r_gen_code.vertex_global,added_vtx);
					r_gen_code.vertex=function_code["vertex"];
				}

				if (fnode->name=="fragment") {

					_dump_function_deps(pnode,fnode->name,function_code,r_gen_code.fragment_global,added_fragment);
					r_gen_code.fragment=function_code["fragment"];
				}

				if (fnode->name=="light") {

					_dump_function_deps(pnode,fnode->name,function_code,r_gen_code.fragment_global,added_fragment);
					r_gen_code.light=function_code["light"];
				}
			}

			//code+=dump_node_code(pnode->body,p_level);
		} break;
		case SL::Node::TYPE_FUNCTION: {

		} break;
		case SL::Node::TYPE_BLOCK: {
			SL::BlockNode *bnode=(SL::BlockNode*)p_node;

			//variables
			code+=_mktab(p_level-1)+"{\n";
			for(Map<StringName,SL::BlockNode::Variable>::Element *E=bnode->variables.front();E;E=E->next()) {

				code+=_mktab(p_level)+_prestr(E->get().precision)+_typestr(E->get().type)+" "+_mkid(E->key())+";\n";
			}

			for(int i=0;i<bnode->statements.size();i++) {

				String scode = _dump_node_code(bnode->statements[i],p_level,r_gen_code,p_actions,p_default_actions);

				if (bnode->statements[i]->type==SL::Node::TYPE_CONTROL_FLOW || bnode->statements[i]->type==SL::Node::TYPE_CONTROL_FLOW) {
					code+=scode; //use directly
				} else {
					code+=_mktab(p_level)+scode+";\n";
				}
			}
			code+=_mktab(p_level-1)+"}\n";


		} break;
		case SL::Node::TYPE_VARIABLE: {
			SL::VariableNode *vnode=(SL::VariableNode*)p_node;

			if (p_default_actions.usage_defines.has(vnode->name) && !used_name_defines.has(vnode->name)) {
				r_gen_code.defines.push_back(p_default_actions.usage_defines[vnode->name].utf8());
				used_name_defines.insert(vnode->name);
			}

			if (p_actions.usage_flag_pointers.has(vnode->name) && !used_name_defines.has(vnode->name)) {
				*p_actions.usage_flag_pointers[vnode->name]=true;
				used_name_defines.insert(vnode->name);
			}

			if (p_default_actions.renames.has(vnode->name))
				code=p_default_actions.renames[vnode->name];
			else
				code=_mkid(vnode->name);


		} break;
		case SL::Node::TYPE_CONSTANT: {
			SL::ConstantNode *cnode=(SL::ConstantNode*)p_node;
			return get_constant_text(cnode->datatype,cnode->values);

		} break;
		case SL::Node::TYPE_OPERATOR: {
			SL::OperatorNode *onode=(SL::OperatorNode*)p_node;


			switch(onode->op) {

				case SL::OP_ASSIGN:
				case SL::OP_ASSIGN_ADD:
				case SL::OP_ASSIGN_SUB:
				case SL::OP_ASSIGN_MUL:
				case SL::OP_ASSIGN_DIV:
				case SL::OP_ASSIGN_SHIFT_LEFT:
				case SL::OP_ASSIGN_SHIFT_RIGHT:
				case SL::OP_ASSIGN_MOD:
				case SL::OP_ASSIGN_BIT_AND:
				case SL::OP_ASSIGN_BIT_OR:
				case SL::OP_ASSIGN_BIT_XOR:
					code=_dump_node_code(onode->arguments[0],p_level,r_gen_code,p_actions,p_default_actions)+_opstr(onode->op)+_dump_node_code(onode->arguments[1],p_level,r_gen_code,p_actions,p_default_actions);
					break;
				case SL::OP_BIT_INVERT:
				case SL::OP_NEGATE:
				case SL::OP_NOT:
				case SL::OP_DECREMENT:
				case SL::OP_INCREMENT:
					code=_opstr(onode->op)+_dump_node_code(onode->arguments[0],p_level,r_gen_code,p_actions,p_default_actions);
					break;
				case SL::OP_POST_DECREMENT:
				case SL::OP_POST_INCREMENT:
					code=_dump_node_code(onode->arguments[0],p_level,r_gen_code,p_actions,p_default_actions)+_opstr(onode->op);
					break;
				case SL::OP_CALL:
				case SL::OP_CONSTRUCT: {

					ERR_FAIL_COND_V(onode->arguments[0]->type!=SL::Node::TYPE_VARIABLE,String());

					SL::VariableNode *vnode=(SL::VariableNode*)onode->arguments[0];

					if (onode->op==SL::OP_CONSTRUCT) {
						code+=String(vnode->name);
					} else {

						if (internal_functions.has(vnode->name)) {
							code+=vnode->name;
						} else if (p_default_actions.renames.has(vnode->name)) {
							code+=p_default_actions.renames[vnode->name];
						} else {
							code+=_mkid(vnode->name);
						}
					}

					code+="(";

					for(int i=1;i<onode->arguments.size();i++) {
						if (i>1)
							code+=", ";
						code+=_dump_node_code(onode->arguments[i],p_level,r_gen_code,p_actions,p_default_actions);
					}
					code+=")";
				} break;
				default: {

					code="("+_dump_node_code(onode->arguments[0],p_level,r_gen_code,p_actions,p_default_actions)+_opstr(onode->op)+_dump_node_code(onode->arguments[1],p_level,r_gen_code,p_actions,p_default_actions)+")";
					break;

				}
			}

		} break;
		case SL::Node::TYPE_CONTROL_FLOW: {
			SL::ControlFlowNode *cfnode=(SL::ControlFlowNode*)p_node;
			if (cfnode->flow_op==SL::FLOW_OP_IF) {

				code+=_mktab(p_level)+"if ("+_dump_node_code(cfnode->expressions[0],p_level,r_gen_code,p_actions,p_default_actions)+")\n";
				code+=_dump_node_code(cfnode->blocks[0],p_level+1,r_gen_code,p_actions,p_default_actions);
				if (cfnode->blocks.size()==2) {

					code+=_mktab(p_level)+"else\n";
					code+=_dump_node_code(cfnode->blocks[1],p_level+1,r_gen_code,p_actions,p_default_actions);
				}


			} else if (cfnode->flow_op==SL::FLOW_OP_RETURN) {

				if (cfnode->blocks.size()) {
					code="return "+_dump_node_code(cfnode->blocks[0],p_level,r_gen_code,p_actions,p_default_actions);
				} else {
					code="return";
				}
			}

		} break;
		case SL::Node::TYPE_MEMBER: {
			SL::MemberNode *mnode=(SL::MemberNode*)p_node;
			code=_dump_node_code(mnode->owner,p_level,r_gen_code,p_actions,p_default_actions)+"."+mnode->name;

		} break;
	}

	return code;

}


Error ShaderCompilerGLES3::compile(VS::ShaderMode p_mode, const String& p_code, IdentifierActions* p_actions, const String &p_path,GeneratedCode& r_gen_code) {



	Error err = parser.compile(p_code,ShaderTypes::get_singleton()->get_functions(p_mode),ShaderTypes::get_singleton()->get_modes(p_mode));

	if (err!=OK) {
		_err_print_error(NULL,p_path.utf8().get_data(),parser.get_error_line(),parser.get_error_text().utf8().get_data(),ERR_HANDLER_SHADER);
		return err;
	}

	r_gen_code.defines.clear();
	r_gen_code.vertex=String();
	r_gen_code.vertex_global=String();
	r_gen_code.fragment=String();
	r_gen_code.fragment_global=String();
	r_gen_code.light=String();



	used_name_defines.clear();
	used_rmode_defines.clear();

	_dump_node_code(parser.get_shader(),1,r_gen_code,*p_actions,actions[p_mode]);

	return OK;

}


ShaderCompilerGLES3::ShaderCompilerGLES3() {

	/** CANVAS ITEM SHADER **/

	actions[VS::SHADER_CANVAS_ITEM].renames["SRC_VERTEX"]="vertex";
	actions[VS::SHADER_CANVAS_ITEM].renames["VERTEX"]="outvec.xy";
	actions[VS::SHADER_CANVAS_ITEM].renames["VERTEX_COLOR"]="vertex_color";
	actions[VS::SHADER_CANVAS_ITEM].renames["UV"]="uv_interp";
	actions[VS::SHADER_CANVAS_ITEM].renames["POINT_SIZE"]="gl_PointSize";

	actions[VS::SHADER_CANVAS_ITEM].renames["WORLD_MATRIX"]="modelview_matrix";
	actions[VS::SHADER_CANVAS_ITEM].renames["PROJECTION_MATRIX"]="projection_matrix";
	actions[VS::SHADER_CANVAS_ITEM].renames["EXTRA_MATRIX"]=="extra_matrix";
	actions[VS::SHADER_CANVAS_ITEM].renames["TIME"]="time";

	actions[VS::SHADER_CANVAS_ITEM].renames["COLOR"]="color";
	actions[VS::SHADER_CANVAS_ITEM].renames["NORMAL"]="normal";
	actions[VS::SHADER_CANVAS_ITEM].renames["NORMALMAP"]="normal_map";
	actions[VS::SHADER_CANVAS_ITEM].renames["NORMALMAP_DEPTH"]="normal_depth";
	actions[VS::SHADER_CANVAS_ITEM].renames["UV"]="uv_interp";
	actions[VS::SHADER_CANVAS_ITEM].renames["COLOR"]="color";
	actions[VS::SHADER_CANVAS_ITEM].renames["TEXTURE"]="color_texture";
	actions[VS::SHADER_CANVAS_ITEM].renames["TEXTURE_PIXEL_SIZE"]="color_texpixel_size";
	actions[VS::SHADER_CANVAS_ITEM].renames["SCREEN_UV"]="screen_uv";
	actions[VS::SHADER_CANVAS_ITEM].renames["SCREEN_TEXTURE"]="screen_texture";
	actions[VS::SHADER_CANVAS_ITEM].renames["POINT_COORD"]="gl_PointCoord";

	actions[VS::SHADER_CANVAS_ITEM].renames["LIGHT_VEC"]="light_vec";
	actions[VS::SHADER_CANVAS_ITEM].renames["LIGHT_HEIGHT"]="light_height";
	actions[VS::SHADER_CANVAS_ITEM].renames["LIGHT_COLOR"]="light_color";
	actions[VS::SHADER_CANVAS_ITEM].renames["LIGHT_UV"]="light_uv";
	//actions[VS::SHADER_CANVAS_ITEM].renames["LIGHT_SHADOW_COLOR"]="light_shadow_color";
	actions[VS::SHADER_CANVAS_ITEM].renames["LIGHT"]="light";
	actions[VS::SHADER_CANVAS_ITEM].renames["SHADOW_COLOR"]="shadow_color";

	actions[VS::SHADER_CANVAS_ITEM].usage_defines["COLOR"]="#define COLOR_USED\n";
	actions[VS::SHADER_CANVAS_ITEM].usage_defines["SCREEN_TEXTURE"]="#define SCREEN_TEXTURE_USED\n";
	actions[VS::SHADER_CANVAS_ITEM].usage_defines["SCREEN_UV"]="#define SCREEN_UV_USED\n";
	actions[VS::SHADER_CANVAS_ITEM].usage_defines["NORMAL"]="#define NORMAL_USED\n";
	actions[VS::SHADER_CANVAS_ITEM].usage_defines["NORMALMAP"]="#define NORMALMAP_USED\n";
	actions[VS::SHADER_CANVAS_ITEM].usage_defines["SHADOW_COLOR"]="#define SHADOW_COLOR_USED\n";

	actions[VS::SHADER_CANVAS_ITEM].render_mode_defines["skip_transform"]="#define SKIP_TRANSFORM_USED\n";

	List<String> func_list;

	ShaderLanguage::get_builtin_funcs(&func_list);

	for (List<String>::Element *E=func_list.front();E;E=E->next()) {
		internal_functions.insert(E->get());
	}
}