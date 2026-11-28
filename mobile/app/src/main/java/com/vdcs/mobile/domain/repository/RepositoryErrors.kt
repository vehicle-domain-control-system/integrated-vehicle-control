package com.vdcs.mobile.domain.repository

class RequestBlocked(reason: String) : IllegalStateException(reason)

class RequestNotSent(reason: String) : IllegalStateException(reason)

class InvalidRequest(reason: String) : IllegalArgumentException(reason)

class PermissionMissing(reason: String) : IllegalStateException(reason)
