#include <lfortran/asr.h>
#include <lfortran/containers.h>
#include <lfortran/exception.h>
#include <lfortran/asr_utils.h>
#include <lfortran/asr_verify.h>
#include <lfortran/pass/pass_utils.h>

namespace LFortran {

    namespace PassUtils {

        int get_rank(ASR::expr_t* x) {
            int n_dims = 0;
            if( x->type == ASR::exprType::Var ) {
                const ASR::symbol_t* x_sym = symbol_get_past_external(
                                                ASR::down_cast<ASR::Var_t>(x)->m_v);
                if( x_sym->type == ASR::symbolType::Variable ) {
                    ASR::Variable_t *v = ASR::down_cast<ASR::Variable_t>(x_sym);
                    ASR::ttype_t* x_type = v->m_type;
                    switch( v->m_type->type ) {
                        case ASR::ttypeType::Integer: {
                            ASR::Integer_t* x_type_ref = ASR::down_cast<ASR::Integer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::IntegerPointer: {
                            ASR::IntegerPointer_t* x_type_ref = ASR::down_cast<ASR::IntegerPointer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::Real: {
                            ASR::Real_t* x_type_ref = ASR::down_cast<ASR::Real_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::RealPointer: {
                            ASR::RealPointer_t* x_type_ref = ASR::down_cast<ASR::RealPointer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::Complex: {
                            ASR::Complex_t* x_type_ref = ASR::down_cast<ASR::Complex_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::ComplexPointer: {
                            ASR::ComplexPointer_t* x_type_ref = ASR::down_cast<ASR::ComplexPointer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::Derived: {
                            ASR::Derived_t* x_type_ref = ASR::down_cast<ASR::Derived_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::DerivedPointer: {
                            ASR::DerivedPointer_t* x_type_ref = ASR::down_cast<ASR::DerivedPointer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::Logical: {
                            ASR::Logical_t* x_type_ref = ASR::down_cast<ASR::Logical_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::LogicalPointer: {
                            ASR::LogicalPointer_t* x_type_ref = ASR::down_cast<ASR::LogicalPointer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::Character: {
                            ASR::Character_t* x_type_ref = ASR::down_cast<ASR::Character_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        case ASR::ttypeType::CharacterPointer: {
                            ASR::CharacterPointer_t* x_type_ref = ASR::down_cast<ASR::CharacterPointer_t>(x_type);
                            n_dims = x_type_ref->n_dims;
                            break;
                        }
                        default:
                            break;
                    }
                }
            }
            return n_dims;
        }   

        bool is_array(ASR::expr_t* x) {
            return get_rank(x) > 0;
        }

        ASR::expr_t* create_array_ref(ASR::expr_t* arr_expr, Vec<ASR::expr_t*>& idx_vars, Allocator& al) {
            ASR::Var_t* arr_var = ASR::down_cast<ASR::Var_t>(arr_expr);
            ASR::symbol_t* arr = arr_var->m_v;
            return create_array_ref(arr, idx_vars, al, arr_expr->base.loc, expr_type(EXPR((ASR::asr_t*)arr_var)));
        }

        ASR::expr_t* create_array_ref(ASR::symbol_t* arr, Vec<ASR::expr_t*>& idx_vars, Allocator& al,
                                      const Location& loc, ASR::ttype_t* _type) {
            Vec<ASR::array_index_t> args;
            args.reserve(al, 1);
            for( size_t i = 0; i < idx_vars.size(); i++ ) {
                ASR::array_index_t ai;
                ai.loc = loc;
                ai.m_left = nullptr;
                ai.m_right = idx_vars[i];
                ai.m_step = nullptr;
                args.push_back(al, ai);
            }
            ASR::expr_t* array_ref = EXPR(ASR::make_ArrayRef_t(al, loc, arr, 
                                                                args.p, args.size(), 
                                                                _type));
            return array_ref;
        }

        void create_idx_vars(Vec<ASR::expr_t*>& idx_vars, int n_dims, const Location& loc, Allocator& al, 
                             ASR::TranslationUnit_t& unit, std::string suffix) {
            idx_vars.reserve(al, n_dims);
            for( int i = 1; i <= n_dims; i++ ) {
                Str str_name;
                str_name.from_str(al, std::to_string(i) + suffix);
                const char* const_idx_var_name = str_name.c_str(al);
                char* idx_var_name = (char*)const_idx_var_name;
                ASR::expr_t* idx_var = nullptr;
                ASR::ttype_t* int32_type = TYPE(ASR::make_Integer_t(al, loc, 4, nullptr, 0));
                ASR::expr_t* const_1 = EXPR(ASR::make_ConstantInteger_t(al, loc, 1, int32_type));
                if( unit.m_global_scope->scope.find(std::string(idx_var_name)) == unit.m_global_scope->scope.end() ) {
                    ASR::asr_t* idx_sym = ASR::make_Variable_t(al, loc, unit.m_global_scope, idx_var_name, 
                                                            ASR::intentType::Local, const_1, ASR::storage_typeType::Default, 
                                                            int32_type, ASR::abiType::Source, ASR::accessType::Public);
                    unit.m_global_scope->scope[std::string(idx_var_name)] = ASR::down_cast<ASR::symbol_t>(idx_sym);
                    idx_var = EXPR(ASR::make_Var_t(al, loc, ASR::down_cast<ASR::symbol_t>(idx_sym)));
                } else {
                    ASR::symbol_t* idx_sym = unit.m_global_scope->scope[std::string(idx_var_name)];
                    idx_var = EXPR(ASR::make_Var_t(al, loc, idx_sym));
                }
                idx_vars.push_back(al, idx_var);
            }
        }

        ASR::expr_t* get_bound(ASR::expr_t* arr_expr, int dim, std::string bound,
                                Allocator& al, ASR::TranslationUnit_t& unit, 
                                SymbolTable*& current_scope) {
            ASR::symbol_t *v;
            std::string remote_sym = bound;
            std::string module_name = "lfortran_intrinsic_array";
            SymbolTable* current_scope_copy = current_scope;
            current_scope = unit.m_global_scope;
            ASR::Module_t *m = load_module(al, current_scope,
                                            module_name, arr_expr->base.loc, true);

            ASR::symbol_t *t = m->m_symtab->resolve_symbol(remote_sym);
            ASR::Function_t *mfn = ASR::down_cast<ASR::Function_t>(t);
            ASR::asr_t *fn = ASR::make_ExternalSymbol_t(al, mfn->base.base.loc, current_scope,
                                                        mfn->m_name, (ASR::symbol_t*)mfn,
                                                        m->m_name, mfn->m_name, ASR::accessType::Private);
            std::string sym = mfn->m_name;
            if( current_scope->scope.find(sym) != current_scope->scope.end() ) {
                v = current_scope->scope[sym];
            } else {
                current_scope->scope[sym] = ASR::down_cast<ASR::symbol_t>(fn);
                v = ASR::down_cast<ASR::symbol_t>(fn);
            }
            Vec<ASR::expr_t*> args;
            args.reserve(al, 2);
            args.push_back(al, arr_expr);
            ASR::expr_t* const_1 = EXPR(ASR::make_ConstantInteger_t(al, arr_expr->base.loc, dim, expr_type(mfn->m_args[1])));
            args.push_back(al, const_1);
            ASR::ttype_t *type = EXPR2VAR(ASR::down_cast<ASR::Function_t>(
                                        symbol_get_past_external(v))->m_return_var)->m_type;
            current_scope = current_scope_copy;
            return EXPR(ASR::make_FunctionCall_t(al, arr_expr->base.loc, v, nullptr,
                                                args.p, args.size(), nullptr, 0, type));
        }

    }

}