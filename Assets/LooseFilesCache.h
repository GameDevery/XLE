// Distributed under the MIT License (See
// accompanying file "LICENSE" or the website
// http://www.opensource.org/licenses/mit-license.php)

#pragma once

#include "IntermediatesStore.h"
#include "ICompileOperation.h"
#include "AssetsCore.h"
#include "../ConsoleRig/GlobalServices.h"
#include "../Utility/IteratorUtils.h"
#include <string>

namespace Assets
{
	class DependentFileState;

	class LooseFilesStorage
	{
	public:
		void StoreCompileProducts(
			StringSection<> archivableName,
			IteratorRange<const ICompileOperation::SerializedArtifact*> artifacts,
			::Assets::AssetState state,
			IteratorRange<const DependentFileState*> dependencies);

		std::shared_ptr<IArtifactCollection> RetrieveCompileProducts(
			StringSection<> archivableName,
			const std::shared_ptr<StoreReferenceCounts>& storeRefCounts,
			uint64_t refCountHashCode);

		LooseFilesStorage(StringSection<> baseDirectory, const ConsoleRig::LibVersionDesc& compilerVersionInfo);
		~LooseFilesStorage();
	private:
		std::string _baseDirectory;
		ConsoleRig::LibVersionDesc _compilerVersionInfo;
		std::string MakeProductsFileName(StringSection<>);
	};
}
