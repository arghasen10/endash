/*
 * ext-callback.h
 *
 *  Created on: 17-Apr-2020
 *      Author: abhijit
 */

#define SRC_SPDASH_MODEL_HTTP_EXT_CALLBACK_H_ //disabling this file
#ifndef SRC_SPDASH_MODEL_HTTP_EXT_CALLBACK_H_
#define SRC_SPDASH_MODEL_HTTP_EXT_CALLBACK_H_

#include "ns3/callback.h"
#include "ns3/object.h"

namespace ns3 {



#define DT0
#define DT1 DT0, T1
#define DT2 DT1, T2
#define DT3 DT2, T3
#define DT4 DT3, T4
#define DT5 DT4, T5
#define DT6 DT5, T6
#define DT7 DT6, T7
#define DT8 DT7, T8

#define DTM0
#define DTM1 DTM0, T1 t1
#define DTM2 DTM1, T2 t2
#define DTM3 DTM2, T3 t3
#define DTM4 DTM3, T4 t4
#define DTM5 DTM4, T5 t5
#define DTM6 DTM5, T6 t6
#define DTM7 DTM6, T7 t7
#define DTM8 DTM7, T8 t8


#define DTN0
#define DTN1 DTN0 T1 t1
#define DTN2 DTN1, T2 t2
#define DTN3 DTN2, T3 t3
#define DTN4 DTN3, T4 t4
#define DTN5 DTN4, T5 t5
#define DTN6 DTN5, T6 t6
#define DTN7 DTN6, T7 t7
#define DTN8 DTN7, T8 t8


#define DTA0
#define DTA1 DTA0, t1
#define DTA2 DTA1, t2
#define DTA3 DTA2, t3
#define DTA4 DTA3, t4
#define DTA5 DTA4, t5
#define DTA6 DTA5, t6
#define DTA7 DTA6, t7
#define DTA8 DTA7, t8

#define DTAX0
#define DTAX1 DTAX0 t1
#define DTAX2 DTAX1, t2
#define DTAX3 DTAX2, t3
#define DTAX4 DTAX3, t4
#define DTAX5 DTAX4, t5
#define DTAX6 DTAX5, t6
#define DTAX7 DTAX6, t7
#define DTAX8 DTAX7, t8

#define DTE0
#define DTE1 DTE0, typename T1=empty
#define DTE2 DTE1, typename T2=empty
#define DTE3 DTE2, typename T3=empty
#define DTE4 DTE3, typename T4=empty
#define DTE5 DTE4, typename T5=empty
#define DTE6 DTE5, typename T6=empty
#define DTE7 DTE6, typename T7=empty
#define DTE8 DTE7, typename T8=empty

#define DEF_CALL0(x) R Call(){m_cb(x);};
#define DEF_CALL1(x) DEF_CALL0(x) R Call(DTN1){m_cb(x DTA1);}
#define DEF_CALL2(x) DEF_CALL1(x) R Call(DTN2){m_cb(x DTA2);}
#define DEF_CALL3(x) DEF_CALL2(x) R Call(DTN3){m_cb(x DTA3);}
#define DEF_CALL4(x) DEF_CALL3(x) R Call(DTN4){m_cb(x DTA4);}
#define DEF_CALL5(x) DEF_CALL4(x) R Call(DTN5){m_cb(x DTA5);}
#define DEF_CALL6(x) DEF_CALL5(x) R Call(DTN6){m_cb(x DTA6);}
#define DEF_CALL7(x) DEF_CALL6(x) R Call(DTN7){m_cb(x DTA7);}
#define DEF_CALL8(x) DEF_CALL7(x) R Call(DTN8){m_cb(x DTA8);}



template <typename T, typename OBJ, typename R,
typename TX DTE8
>
class TmpObj: public Object {
public:
	TmpObj(R (T::*memPtr)(TX), OBJ objPtr, TX tx): m_tx(tx) {
		m_cb = MakeCallback(memPtr, objPtr);
	}
	DEF_CALL8(m_tx)

private:
	Callback<R DT8> m_cb;
	TX m_tx;
};



#define MAKE_MAKE_EXT_CALL_BACK_N(x) \
template <typename T, typename O, typename R, \
typename TX DTE##x> \
R CallCB(Ptr<TmpObj<T, O, R, TX DT##x> > obj DTM##x){ \
	return obj->Call(DTAX##x); \
} \
\
template <typename T, typename O, typename R, \
typename TX DTE##x> \
Callback<R> MakeExtCallback (R (T::*memPtr)(TX DT##x), O objPtr, TX arg DTM##x) \
{ \
	auto obj = Create<TmpObj<T, O, R, TX DT##x> >(memPtr, objPtr, arg); \
	return MakeBoundCallback<R>(CallCB<T, O, R, TX DT##x>, obj DTA##x); \
}

MAKE_MAKE_EXT_CALL_BACK_N(0)
MAKE_MAKE_EXT_CALL_BACK_N(1)


}

#endif /* SRC_SPDASH_MODEL_HTTP_EXT_CALLBACK_H_ */
