#pragma once
#ifndef CONTEXT_H
#define CONTEXT_H

#include "Core.h"

namespace Boom
{
	// Forward declaration of the base interface
	struct AppInterface;

	
	/**
	* @brief Holds global state and owns all attached layers.
	*/
	struct AppContext
	{
		/// BOOM_INLINE hints to the compiler to inline destructor calls
		/// reducing function-call overhead in the engine’s core update loop

		/** @brief Destructor that deletes and nulls out all layer pointers. */ 
		BOOM_INLINE ~AppContext()
		{
			// Iterate and delete each layer, then null out pointer
			for (auto& layer : Layers)
			{
				BOOM_DELETE(layer);
			}
		}

		/**
		 * @brief Container of all active layers in the application.
		 *
		 * Stores pointers to AppInterface-derived layers. Pointers
		 * are managed manually and cleaned up in the destructor.
		 */
		std::vector<AppInterface*> Layers; //Use of pointers within containers to prevent memory leaks and promote safe practice
	};

}// namespace Boom


#endif // CONTEXT_H
