#pragma once

namespace ActorActivationHandler
{
    /// Register the activation event sink. Call during plugin load.
    /// Feature enable/disable is controlled via Config::ShowPopupForActorsInScene()
    void Register();
}
