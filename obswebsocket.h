
enum WebSocketOpCode {
    Hello = 0,
    Identify = 1,
    Identified = 2,
    Reidentify = 3,
    Event = 5,
    Request = 6,
    RequestResponse = 7,
    RequestBatch = 8,
    RequestBatchResponse = 9,
};

enum WebSocketCloseCode {
    // Internal only
    DontClose = 0,
    // Reserved
    UnknownReason = 4000,
    // The server was unable to decode the incoming websocket message
    MessageDecodeError = 4002,
    // A data key is missing but required
    MissingDataKey = 4003,
    // A data key has an invalid type
    InvalidDataKeyType = 4004,
    // The specified `op` was invalid or missing
    UnknownOpCode = 4005,
    // The client sent a websocket message without first sending `Identify` message
    NotIdentified = 4006,
    // The client sent an `Identify` message while already identified
    AlreadyIdentified = 4007,
    // The authentication attempt (via `Identify`) failed
    AuthenticationFailed = 4008,
    // The server detected the usage of an old version of the obs-websocket protocol.
    UnsupportedRpcVersion = 4009,
    // The websocket session has been invalidated by the obs-websocket server.
    SessionInvalidated = 4010,
};

enum EventSubscription {
    // Set subscriptions to 0 to disable all events
    None = 0,
    // Receive events in the `General` category
    General = (1 << 0),
    // Receive events in the `Config` category
    Config = (1 << 1),
    // Receive events in the `Scenes` category
    Scenes = (1 << 2),
    // Receive events in the `Inputs` category
    Inputs = (1 << 3),
    // Receive events in the `Transitions` category
    Transitions = (1 << 4),
    // Receive events in the `Filters` category
    Filters = (1 << 5),
    // Receive events in the `Outputs` category
    Outputs = (1 << 6),
    // Receive events in the `Scene Items` category
    SceneItems = (1 << 7),
    // Receive events in the `MediaInputs` category
    MediaInputs = (1 << 8),
    // Receive all event categories
    All = (General | Config | Scenes | Inputs | Transitions | Filters | Outputs | SceneItems | MediaInputs),
    // InputVolumeMeters event (high-volume)
    InputVolumeMeters = (1 << 9),
    // InputActiveStateChanged event (high-volume)
    InputActiveStateChanged = (1 << 10),
    // InputShowStateChanged event (high-volume)
    InputShowStateChanged = (1 << 11),
};

enum RequestStatus {
    Unknown = 0,
    // For internal use to signify a successful parameter check
    NoError = 10,
    Success = 100,
    // The `requestType` field is missing from the request data
    MissingRequestType = 203,
    // The request type is invalid or does not exist
    UnknownRequestType = 204,
    // Generic error code (comment required)
    GenericError = 205,
    // A required request parameter is missing
    MissingRequestParameter = 300,
    // The request does not have a valid requestData object.
    MissingRequestData = 301,
    // Generic invalid request parameter message (comment required)
    InvalidRequestParameter = 400,
    // A request parameter has the wrong data type
    InvalidRequestParameterType = 401,
    // A request parameter (float or int) is out of valid range
    RequestParameterOutOfRange = 402,
    // A request parameter (string or array) is empty and cannot be
    RequestParameterEmpty = 403,
    // There are too many request parameters (eg. a request takes two optionals, where only one is allowed at a time)
    TooManyRequestParameters = 404,
    // An output is running and cannot be in order to perform the request (generic)
    OutputRunning = 500,
    // An output is not running and should be
    OutputNotRunning = 501,
    // An output is paused and should not be
    OutputPaused = 502,
    // An output is disabled and should not be
    OutputDisabled = 503,
    // Studio mode is active and cannot be
    StudioModeActive = 504,
    // Studio mode is not active and should be
    StudioModeNotActive = 505,
    // The resource was not found
    ResourceNotFound = 600,
    // The resource already exists
    ResourceAlreadyExists = 601,
    // The type of resource found is invalid
    InvalidResourceType = 602,
    // There are not enough instances of the resource in order to perform the request
    NotEnoughResources = 603,
    // The state of the resource is invalid. For example, if the resource is blocked from being accessed
    InvalidResourceState = 604,
    // The specified input (obs_source_t-OBS_SOURCE_TYPE_INPUT) had the wrong kind
    InvalidInputKind = 605,

    // Creating the resource failed
    ResourceCreationFailed = 700,
    // Performing an action on the resource failed
    ResourceActionFailed = 701,
    // Processing the request failed unexpectedly (comment required)
    RequestProcessingFailed = 702,
    // The combination of request parameters cannot be used to perform an action
    CannotAct = 703,
};
