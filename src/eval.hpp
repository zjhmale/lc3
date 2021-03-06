#pragma once

#include "ast.hpp"
#include "object.hpp"
#include "builtins.hpp"
#include "quote_unquote.hpp"
#include "modify.hpp"
#include <map>
#include <vector>
#include <memory>
#ifndef FORMAT_HEADER
#define FORMAT_HEADER
#include <fmt/format.h>
#include <fmt/format.cc>
#endif

using namespace std;
using namespace ast;
using namespace fmt;
using namespace object;
using namespace modify;
using namespace quoteunquote;

namespace eval {
  shared_ptr<Object> NULLOBJ = make_shared<Null>();
  shared_ptr<Object> TRUEOBJ = make_shared<object::Boolean>(true);
  shared_ptr<Object> FALSEOBJ = make_shared<object::Boolean>(false);

  auto eval(shared_ptr<Node> node, shared_ptr<Environment> env) -> shared_ptr<Object>;

  auto quote(shared_ptr<Node> node, shared_ptr<Environment> env) -> shared_ptr<Object>;

  auto eval_unquote_calls(shared_ptr<Node> quoted, shared_ptr<Environment> env) -> shared_ptr<Node> {
    return modify::modify(quoted, [&](shared_ptr<Node> node) -> shared_ptr<Node> {
        if (!is_unquote_call(node)) {
          return node;
        }

        auto call = static_pointer_cast<CallExpression>(node);
        if (call->arguments.size() != 1) {
          return node;
        }

        auto unquoted = eval(call->arguments[0], env);
        return convert_object_to_node(unquoted);
      });
  }

  auto quote(shared_ptr<Node> node, shared_ptr<Environment> env) -> shared_ptr<Object> {
    auto new_node = eval_unquote_calls(node, env);
    return make_shared<Quote>(new_node);
  }

  auto eval_program(shared_ptr<Program> program, shared_ptr<Environment> env) -> shared_ptr<Object> {
    shared_ptr<Object> result;

    auto stmts = program->statements;
    for (const auto &stmt : stmts) {
      result = eval(stmt, env);

      if (result != nullptr) {
        if (result->type() == RETURN_VALUE_OBJ) {
          return static_pointer_cast<ReturnValue>(result)->value;
        } else if (result->type() == ERROR_OBJ) {
          return result;
        }
      }
    }

    return result;
  }

  auto eval_block_statement(shared_ptr<BlockStatement> block, shared_ptr<Environment> env) {
    shared_ptr<Object> result;

    auto stmts = block->statements;
    for (const auto &stmt : stmts) {
      result = eval(stmt, env);

      if (result != nullptr) {
        auto rt = result->type();
        if (rt == RETURN_VALUE_OBJ || rt == ERROR_OBJ) {
          return result;
        }
      }
    }

    return result;
  }

  auto is_error(shared_ptr<Object> o) -> bool {
    if (o != nullptr) {
      return o->type() == ERROR_OBJ;
    } else {
      return false;
    }
  }

  auto is_truthy(shared_ptr<Object> obj) -> bool {
    if (obj->type() == BOOLEAN_OBJ) {
      return static_pointer_cast<object::Boolean>(obj)->value;
    } else if (obj->type() == NULL_OBJ) {
      return false;
    } else {
      return true;
    }
  }

  auto trans_boolean_object(bool input) -> shared_ptr<Object> {
    return input ? TRUEOBJ : FALSEOBJ;
  }

  auto eval_bang_operator_expression(shared_ptr<Object> right) -> shared_ptr<Object> {
    if (right->type() == BOOLEAN_OBJ) {
      shared_ptr<object::Boolean> bo = static_pointer_cast<object::Boolean>(right);
      if (bo->value) {
        return FALSEOBJ;
      } else {
        return TRUEOBJ;
      }
    } else if (right->type() == NULL_OBJ) {
      return TRUEOBJ;
    } else {
      return FALSEOBJ;
    }
  }

  auto eval_minus_prefix_operator_expression(shared_ptr<Object> right) -> shared_ptr<Object> {
    if (right->type() != INTEGER_OBJ) {
      return make_shared<Error>(format("unknown operator: -{0}", right->type()));
    } else {
      auto val = static_pointer_cast<Integer>(right)->value;
      return make_shared<Integer>(-val);
    }
  }

  auto eval_prefix_expression(string prefix_operator, shared_ptr<Object> right) -> shared_ptr<Object> {
    if (prefix_operator == "!") {
      return eval_bang_operator_expression(right);
    } else if (prefix_operator == "-") {
      return eval_minus_prefix_operator_expression(right);
    } else {
      return make_shared<Error>(format("unknown prefix_operator: {0}{1}", prefix_operator, right->type()));
    }
  }

  auto eval_integer_infix_expression(string infix_operator,
                                     shared_ptr<Object> left,
                                     shared_ptr<Object> right) -> shared_ptr<Object> {
    auto left_int = static_pointer_cast<Integer>(left)->value;
    auto right_int = static_pointer_cast<Integer>(right)->value;

    if (infix_operator == "+") {
      return make_shared<Integer>(left_int + right_int);
    } else if (infix_operator == "-") {
      return make_shared<Integer>(left_int - right_int);
    } else if (infix_operator == "*") {
      return make_shared<Integer>(left_int * right_int);
    } else if (infix_operator == "/") {
      return make_shared<Integer>(left_int / right_int);
    } else if (infix_operator == "<") {
      return trans_boolean_object(left_int < right_int);
    } else if (infix_operator == ">") {
      return trans_boolean_object(left_int > right_int);
    } else if (infix_operator == "==") {
      return trans_boolean_object(left_int == right_int);
    } else if (infix_operator == "!=") {
      return trans_boolean_object(left_int != right_int);
    } else {
      return make_shared<Error>(format("unknown operator: {0} {1} {2}", left->type(), infix_operator, right->type()));
    }
  }

  auto eval_string_infix_expression(string infix_operator,
                                    shared_ptr<Object> left,
                                    shared_ptr<Object> right) -> shared_ptr<Object> {
    if (infix_operator == "+") {
      auto left_str = static_pointer_cast<String>(left)->value;
      auto right_str = static_pointer_cast<String>(right)->value;
      return make_shared<String>(left_str + right_str);
    } else {
      return make_shared<Error>(format("unknown operator: {0} {1} {2}", left->type(), infix_operator, right->type()));
    }
  }

  auto eval_infix_expression(string infix_operator,
                             shared_ptr<Object> left,
                             shared_ptr<Object> right) -> shared_ptr<Object> {
    if (left->type() == INTEGER_OBJ && right->type() == INTEGER_OBJ) {
      return eval_integer_infix_expression(infix_operator, left, right);
    } else if (left->type() == STRING_OBJ && right->type() == STRING_OBJ) {
      return eval_string_infix_expression(infix_operator, left, right);
    } else if (infix_operator == "==") {
      return trans_boolean_object(left == right);
    } else if (infix_operator == "!=") {
      return trans_boolean_object(left != right);
    } else if (left->type() != right->type()) {
      return make_shared<Error>(format("type mismatch: {0} {1} {2}", left->type(), infix_operator, right->type()));
    } else {
      return make_shared<Error>(format("unknown operator: {0} {1} {2}", left->type(), infix_operator, right->type()));
    }
  }

  auto eval_if_expression(shared_ptr<IfExpression> if_expr, shared_ptr<Environment> env) -> shared_ptr<Object> {
    auto condition = eval(if_expr->condition, env);
    if (is_error(condition)) {
      return condition;
    }

    if (is_truthy(condition)) {
      return eval(if_expr->consequence, env);
    } else if (if_expr->alternative != nullptr) {
      return eval(if_expr->alternative, env);
    } else {
      return NULLOBJ;
    }
  }

  auto eval_identifier(shared_ptr<Identifier> id_expr, shared_ptr<Environment> env) -> shared_ptr<Object> {
    auto val = env->get(id_expr->value);
    if (val != nullptr) {
      return val;
    }

    auto builtin = builtins::builtins[id_expr->value];
    if (builtin != nullptr) {
      return builtin;
    }

    return make_shared<Error>(format("identifier not found: {0}", id_expr->value));
  }

  auto eval_expressions(vector<shared_ptr<Expression>> exprs, shared_ptr<Environment> env) {
    vector<shared_ptr<Object>> result = {};
    for (size_t i = 0; i < exprs.size(); i++) {
      auto evaluated = eval(exprs[i], env);
      if (is_error(evaluated)) {
        return vector<shared_ptr<Object>>({ evaluated });
      }
      result.push_back(evaluated);
    }
    return result;
  }

  auto extend_function_env(shared_ptr<object::Function> func, vector<shared_ptr<Object>> args) -> shared_ptr<Environment> {
    auto env = new_enclosed_environment(func->env);
    for (size_t i = 0; i < func->parameters.size(); i++) {
      env->set(func->parameters[i]->value, args[i]);
    }

    return env;
  }

  auto unwrap_return_value(shared_ptr<Object> obj) -> shared_ptr<Object> {
    if (obj->type() == RETURN_VALUE_OBJ) {
      return static_pointer_cast<ReturnValue>(obj)->value;
    } else {
      return obj;
    }
  }

  auto apply_function(shared_ptr<Object> obj, vector<shared_ptr<Object>> args) -> shared_ptr<Object> {
    if (obj->type() == FUNCTION_OBJ) {
      auto func = static_pointer_cast<object::Function>(obj);
      auto extended_env = extend_function_env(func, args);
      auto evaluated = eval(func->body, extended_env);
      return unwrap_return_value(evaluated);
    } else if (obj->type() == BUILTIN_OBJ) {
      auto builtin = static_pointer_cast<Builtin>(obj);
      return builtin->func(args);
    } else {
      return make_shared<Error>(format("not a function: {0}", obj->type()));
    }
  }

  auto eval_array_index_expression(shared_ptr<Array> arr, shared_ptr<Integer> index) -> shared_ptr<Object> {
    auto max = static_cast<int>(arr->elements.size() - 1);
    auto idx = index->value;
    if (idx < 0 || idx > max) {
      return NULLOBJ;
    }

    return arr->elements[idx];
  }

  auto eval_hash_index_expression(shared_ptr<Hash> hash, shared_ptr<Object> index) -> shared_ptr<Object> {
    if (is_hashable(index)) {
      auto pairs = hash->pairs;
      auto key = dynamic_pointer_cast<Hashable>(index)->hash_key();
      auto result = pairs.find(key);
      if (result != pairs.end()) {
        return result->second.second;
      } else {
        return NULLOBJ;
      }
    } else {
      return make_shared<Error>(format("unusable as hash key: {0}", index->type()));
    }
  }

  auto eval_index_expression(shared_ptr<Object> left, shared_ptr<Object> index) -> shared_ptr<Object> {
    if (left->type() == ARRAY_OBJ && index->type() == INTEGER_OBJ) {
      return eval_array_index_expression(static_pointer_cast<Array>(left),
                                         static_pointer_cast<Integer>(index));
    } else if (left->type() == HASH_OBJ) {
      return eval_hash_index_expression(static_pointer_cast<Hash>(left), index);
    } else {
      return make_shared<Error>(format("index operator not supported: {0}", left->type()));
    }
  }

  auto eval_hash_literal(shared_ptr<HashLiteral> hash_expr, shared_ptr<Environment> env) -> shared_ptr<Object> {
    map<HashKey, HashPair> pairs = {};
    for (auto iter = hash_expr->pairs.begin(); iter != hash_expr->pairs.end(); iter++) {
      auto key_obj = eval(iter->first, env);
      if (is_error(key_obj)) {
        return key_obj;
      }

      if (!is_hashable(key_obj)) {
        return make_shared<Error>(format("unusable as hash key_obj: {0}", key_obj->type()));
      }

      auto value_obj = eval(iter->second, env);
      if (is_error(value_obj)) {
        return value_obj;
      }

      auto hashed = dynamic_pointer_cast<Hashable>(key_obj)->hash_key();
      pairs[hashed] = make_pair(key_obj, value_obj);
    }
    return make_shared<Hash>(pairs);
}

  auto eval(shared_ptr<Node> node, shared_ptr<Environment> env) -> shared_ptr<Object> {
    switch (node->type()) {
    case NodeType::PROGRAM:
      return eval_program(static_pointer_cast<Program>(node), env);
    case NodeType::BLOCKSTATEMENT:
      return eval_block_statement(static_pointer_cast<BlockStatement>(node), env);
    case NodeType::EXPRESSIONSTATEMENT:
      return eval(static_pointer_cast<ExpressionStatement>(node)->expression, env);
    case NodeType::RETURNSTATEMENT: {
      auto val = eval(static_pointer_cast<ReturnStatement>(node)->value, env);
      if (is_error(val)) {
        return val;
      } else {
        return make_shared<ReturnValue>(val);
      }
    }
    case NodeType::LETSTATEMENT: {
      auto let = static_pointer_cast<LetStatement>(node);
      auto val = eval(let->value, env);
      if (is_error(val)) {
        return val;
      } else {
        return env->set(let->name->value, val);
      }
    }
    case NodeType::INTEGERLITERAL: {
      return make_shared<Integer>(static_pointer_cast<IntegerLiteral>(node)->value);
    }
    case NodeType::STRINGLITERAL:
      return make_shared<String>(static_pointer_cast<StringLiteral>(node)->value);
    case NodeType::BOOLEAN:
      return trans_boolean_object(static_pointer_cast<ast::Boolean>(node)->value);
    case NodeType::PREFIXEXPRESSION: {
      auto prefix = static_pointer_cast<PrefixExpression>(node);
      auto right = eval(prefix->right, env);
      if (is_error(right)) {
        return right;
      } else {
        return eval_prefix_expression(prefix->prefix_operator, right);
      }
    }
    case NodeType::INFIXEXPRESSION: {
      auto infix = static_pointer_cast<InfixExpression>(node);
      auto left = eval(infix->left, env);
      if (is_error(left)) {
        return left;
      }
      auto right = eval(infix->right, env);
      if (is_error(right)) {
        return right;
      }

      return eval_infix_expression(infix->infix_operator, left, right);
    }
    case NodeType::IFEXPRESSION:
      return eval_if_expression(static_pointer_cast<IfExpression>(node), env);
    case NodeType::IDENTIFIER:
      return eval_identifier(static_pointer_cast<Identifier>(node), env);
    case NodeType::FUNCTIONLITERAL: {
      auto func_expr = static_pointer_cast<FunctionLiteral>(node);
      auto params = func_expr->parameters;
      auto body = func_expr->body;
      // closure here, save the current context
      return make_shared<object::Function>(params, body, env);
    }
    case NodeType::CALLEXPRESSION: {
      auto call_expr = static_pointer_cast<CallExpression>(node);

      if (call_expr->function->token_literal() == "quote") {
        return quote(call_expr->arguments[0], env);
      }

      auto func_obj = eval(call_expr->function, env);
      if (is_error(func_obj)) {
        return func_obj;
      }

      auto args = eval_expressions(call_expr->arguments, env);
      if (args.size() == 1 && is_error(args[0])) {
        return args[0];
      }

      return apply_function(func_obj, args);
    }
    case NodeType::ARRAYLITERAL: {
      auto arr_expr = static_pointer_cast<ArrayLiteral>(node);
      auto elements = eval_expressions(arr_expr->elements, env);
      if (elements.size() == 1 && is_error(elements[0])) {
        return elements[0];
      }
      return make_shared<Array>(elements);
    }
    case NodeType::INDEXEXPRESSION: {
      auto index_expr = static_pointer_cast<IndexExpression>(node);
      auto left_obj = eval(index_expr->left, env);
      if (is_error(left_obj)) {
        return left_obj;
      }
      auto index_obj = eval(index_expr->index, env);
      if (is_error(index_obj)) {
        return index_obj;
      }
      return eval_index_expression(left_obj, index_obj);
    }
    case NodeType::HASHLITERAL:
      return eval_hash_literal(static_pointer_cast<HashLiteral>(node), env);
    default:
      return nullptr;
    }
  }
}
