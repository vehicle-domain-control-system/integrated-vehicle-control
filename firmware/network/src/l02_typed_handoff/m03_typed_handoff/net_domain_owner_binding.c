#include "network/net_domain_c2.h"
#include <string.h>
/* References Owner implementations, never defines or changes their ABI. This
 * object is pulled from the archive only when this explicit binder is used. */
void net_domain_c2_pinned_owner_api(NetDomainC2Api *out)
{
    if(out==NULL)return;
    memset(out,0,sizeof(*out));
    out->IsInitialized=DomainIf_IsInitialized;
    out->SetRxObserver=DomainIf_SetRxObserver;
    out->SetTxPort=DomainIf_SetTxPort;
    out->RxBcmDoorState=DomainIf_RxBcmDoorState;
    out->RxBcmStatus=DomainIf_RxBcmStatus;
    out->RxBcmCommandResult=DomainIf_RxBcmCommandResult;
    out->RxCisRear=DomainIf_RxCisRear;
    out->RxCisStatus=DomainIf_RxCisStatus;
    out->RxCisCabinEnvironment=DomainIf_RxCisCabinEnvironment;
    out->RxNetworkLocalFailure=DomainIf_RxNetworkLocalFailure;
    out->QueryGetBcmDoorState=DomainIf_QueryGetBcmDoorState;
    out->QueryGetBcmStatus=DomainIf_QueryGetBcmStatus;
    out->QueryGetCisRear=DomainIf_QueryGetCisRear;
    out->QueryGetCisStatus=DomainIf_QueryGetCisStatus;
    out->QueryGetCisEnvironment=DomainIf_QueryGetCisEnvironment;
    out->QueryGetRequestResult=DomainIf_QueryGetRequestResult;
    out->QueryGetRecentRequestResults=DomainIf_QueryGetRecentRequestResults;
}
