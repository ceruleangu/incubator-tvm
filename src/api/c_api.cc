/*!
 *  Copyright (c) 2016 by Contributors
 * Implementation of C API
 * \file c_api.cc
 */
#include <dmlc/base.h>
#include <dmlc/logging.h>
#include <dmlc/thread_local.h>
#include <tvm/c_api.h>
#include <tvm/api_registry.h>
#include <vector>
#include <string>
#include <exception>
#include "../runtime/runtime_base.h"


/*! \brief entry to to easily hold returning information */
struct TVMAPIThreadLocalEntry {
  /*! \brief result holder for returning strings */
  std::vector<std::string> ret_vec_str;
  /*! \brief result holder for returning string pointers */
  std::vector<const char *> ret_vec_charp;
  /*! \brief result holder for retruning string */
  std::string ret_str;
};

using namespace tvm;

/*! \brief Thread local store that can be used to hold return values. */
typedef dmlc::ThreadLocalStore<TVMAPIThreadLocalEntry> TVMAPIThreadLocalStore;

using TVMAPINode = std::shared_ptr<Node>;

struct APIAttrGetter : public AttrVisitor {
  std::string skey;
  TVMRetValue* ret;
  bool found_node_ref{false};

  void Visit(const char* key, double* value) final {
    if (skey == key) *ret = value[0];
  }
  void Visit(const char* key, int64_t* value) final {
    if (skey == key) *ret = value[0];
  }
  void Visit(const char* key, uint64_t* value) final {
    CHECK_LE(value[0], static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
        << "cannot return too big constant";
    if (skey == key) *ret = static_cast<int64_t>(value[0]);
  }
  void Visit(const char* key, int* value) final {
    if (skey == key) *ret = static_cast<int64_t>(value[0]);
  }
  void Visit(const char* key, bool* value) final {
    if (skey == key) *ret = static_cast<int64_t>(value[0]);
  }
  void Visit(const char* key, Type* value) final {
    if (skey == key) *ret = value[0];
  }
  void Visit(const char* key, std::string* value) final {
    if (skey == key) *ret = value[0];
  }
  void Visit(const char* key, NodeRef* value) final {
    if (skey == key) {
      *ret = value[0];
      found_node_ref = true;
    }
  }
};

struct APIAttrDir : public AttrVisitor {
  std::vector<std::string>* names;

  void Visit(const char* key, double* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, int64_t* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, uint64_t* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, bool* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, int* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, Type* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, std::string* value) final {
    names->push_back(key);
  }
  void Visit(const char* key, NodeRef* value) final {
    names->push_back(key);
  }
};


int TVMNodeFree(NodeHandle handle) {
  API_BEGIN();
  delete static_cast<TVMAPINode*>(handle);
  API_END();
}

int TVMCbArgToReturn(TVMValue* value, int code) {
  API_BEGIN();
  tvm::runtime::TVMRetValue rv;
  rv = tvm::runtime::TVMArgValue(*value, code);
  int tcode;
  rv.MoveToCHost(value, &tcode);
  CHECK_EQ(tcode, code);
  API_END();
}

int TVMNodeDupe(NodeHandle handle, NodeHandle* out_handle) {
  API_BEGIN();

  *out_handle = new TVMAPINode(*static_cast<TVMAPINode*>(handle));
  API_END();
}

int TVMNodeGetAttr(NodeHandle handle,
                   const char* key,
                   TVMValue* ret_val,
                   int* ret_type_code,
                   int* ret_success) {
  API_BEGIN();
  TVMRetValue rv;
  APIAttrGetter getter;
  getter.skey = key;
  getter.ret = &rv;
  TVMAPINode* tnode = static_cast<TVMAPINode*>(handle);
  if (getter.skey == "type_key") {
    ret_val->v_str = (*tnode)->type_key();
    *ret_type_code = kStr;
    *ret_success = 1;
  } else {
    (*tnode)->VisitAttrs(&getter);
    *ret_success = getter.found_node_ref || rv.type_code() != kNull;
    if (rv.type_code() == kStr ||
        rv.type_code() == kTVMType) {
      TVMAPIThreadLocalEntry *e = TVMAPIThreadLocalStore::Get();
      e->ret_str = rv.operator std::string();
      *ret_type_code = kStr;
      ret_val->v_str = e->ret_str.c_str();
    } else {
      rv.MoveToCHost(ret_val, ret_type_code);
    }
  }
  API_END();
}

int TVMNodeListAttrNames(NodeHandle handle,
                         int *out_size,
                         const char*** out_array) {
  TVMAPIThreadLocalEntry *ret = TVMAPIThreadLocalStore::Get();
  API_BEGIN();
  ret->ret_vec_str.clear();
  TVMAPINode* tnode = static_cast<TVMAPINode*>(handle);
  APIAttrDir dir;
  dir.names = &(ret->ret_vec_str);
  (*tnode)->VisitAttrs(&dir);
  ret->ret_vec_charp.clear();
  for (size_t i = 0; i < ret->ret_vec_str.size(); ++i) {
    ret->ret_vec_charp.push_back(ret->ret_vec_str[i].c_str());
  }
  *out_array = dmlc::BeginPtr(ret->ret_vec_charp);
  *out_size = static_cast<int>(ret->ret_vec_str.size());
  API_END();
}
