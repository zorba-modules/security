/*
 * Copyright 2006-2008 The FLWOR Foundation.
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sstream>
#include <map>

#include <zorba/base64.h>
#include <zorba/diagnostic_list.h>
#include <zorba/external_module.h>
#include <zorba/user_exception.h>
#include <zorba/item_factory.h>
#include <zorba/singleton_item_sequence.h>
#include <zorba/xquery_exception.h>
#include <zorba/zorba.h>
#include "hash.h"

#include "md5_impl.h"
#include "sha1.h"

namespace zorba { namespace security {

zorba::String getOneStringArgument(
    const HashModule* aModule,
    const ExternalFunction::Arguments_t& aArgs,
    int aIndex)
{
  zorba::Item lItem;
  Iterator_t args_iter = aArgs[aIndex]->getIterator();
  args_iter->open();
  args_iter->next(lItem); // must have one because the signature is defined like this
  zorba::String lTmpString = lItem.getStringValue();
  args_iter->close();
  return lTmpString;
}

HashModule::~HashModule()
{
  for (FuncMap_t::const_iterator lIter = theFunctions.begin();
       lIter != theFunctions.end(); ++lIter) {
    delete lIter->second;
  }
  theFunctions.clear();
}


void
HashModule::destroy()
{
  if (!dynamic_cast<HashModule*>(this)) {
    return;
  }
  delete this;
}

class HashFunction : public NonContextualExternalFunction
{
protected:
  const HashModule* theModule;

public:
  HashFunction(const HashModule* aModule): theModule(aModule){}
  ~HashFunction(){}

  virtual String
  getLocalName() const { return "hash-impl"; }

  virtual zorba::ItemSequence_t
  evaluate(const Arguments_t& aArgs) const
  {
    zorba::String lText = getOneStringArgument(theModule, aArgs, 0);
    zorba::String lAlg = getOneStringArgument(theModule, aArgs, 1);
    zorba::String lHash;
    if (lAlg == "sha1") {
      CSHA1 lSha1;
      const unsigned char* lData = (const unsigned char*) lText.c_str();
      lSha1.Update(lData, lText.size());
      lSha1.Final();
      char lRes[65];
      lSha1.GetHash((UINT_8 *)lRes);

      // SHA1 is always 20bytes long
      // avoid using a stream here because it might contain 0's
      // (i.e. be null terminated)
      std::stringstream lTmp;
      lTmp.write(lRes, 20);

      lHash = zorba::encoding::Base64::encode(lTmp);
    } else {
      lHash = md5(lText.str());
    }
    // implement here
    zorba::Item lItem;
    lItem = theModule->getItemFactory()->createString(lHash);
    return zorba::ItemSequence_t(new zorba::SingletonItemSequence(lItem));
  }

  virtual String
  getURI() const
  {
    return theModule->getURI();
  }

};

ExternalFunction* HashModule::getExternalFunction(const 
    String& aLocalname)
{
  ExternalFunction*& lFunc = theFunctions[aLocalname];
  if (!lFunc) {
    if (!aLocalname.compare("hash-impl")) {
      lFunc = new HashFunction(this);
    }
  }
  return lFunc;
}

ItemFactory* HashModule::theFactory = 0;

} /* namespace security */
} /* namespace zorba */

#ifdef WIN32
#  define DLL_EXPORT __declspec(dllexport)
#else
#  define DLL_EXPORT __attribute__ ((visibility("default")))
#endif

extern "C" DLL_EXPORT zorba::ExternalModule* createModule() {
  return new zorba::security::HashModule();
}
