/*
 *  Control.cs
 *
 *  Copyright 2014 Michael Zillgith
 *
 *  This file is part of libIEC61850.
 *
 *  libIEC61850 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  libIEC61850 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with libIEC61850.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  See COPYING file for the complete license text.
 */

using System;
using System.Runtime.InteropServices;

using IEC61850.Common;

namespace IEC61850
{

    /// <summary>
    /// IEC 61850 common API parts (used by client and server API)
    /// </summary>
	namespace Common {

        /// <summary>
        /// Control model
        /// </summary>
		public enum ControlModel {
			CONTROL_MODEL_STATUS_ONLY = 0,
			CONTROL_MODEL_DIRECT_NORMAL = 1,
			CONTROL_MODEL_SBO_NORMAL = 2,
			CONTROL_MODEL_DIRECT_ENHANCED = 3,
			CONTROL_MODEL_SBO_ENHANCED = 4
		}

        /// <summary>
        /// Originator category
        /// </summary>
        public enum OrCat {
            /** Not supported - should not be used */
            NOT_SUPPORTED = 0,
            /** Control operation issued from an operator using a client located at bay level */
            BAY_CONTROL = 1,
            /** Control operation issued from an operator using a client located at station level */
            STATION_CONTROL = 2,
            /** Control operation from a remote operator outside the substation (for example network control center) */
            REMOTE_CONTROL = 3,
            /** Control operation issued from an automatic function at bay level */
            AUTOMATIC_BAY = 4,
            /** Control operation issued from an automatic function at station level */
            AUTOMATIC_STATION = 5,
            /** Control operation issued from a automatic function outside of the substation */
            AUTOMATIC_REMOTE = 6,
            /** Control operation issued from a maintenance/service tool */
            MAINTENANCE = 7,
            /** Status change occurred without control action (for example external trip of a circuit breaker or failure inside the breaker) */
            PROCESS = 8
        }
	}

	namespace Client {

        /// <summary>
        /// Control object.
        /// </summary>
		public class ControlObject
		{
			[DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
			private static extern IntPtr ControlObjectClient_create(string objectReference, IntPtr connection);

			[DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
			private static extern void ControlObjectClient_destroy(IntPtr self);

			[DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
			private static extern int ControlObjectClient_getControlModel(IntPtr self);

			[DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
			private static extern bool ControlObjectClient_operate(IntPtr self, IntPtr ctlVal, UInt64 operTime);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern bool ControlObjectClient_select(IntPtr self);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern bool ControlObjectClient_selectWithValue(IntPtr self, IntPtr ctlVal);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern bool ControlObjectClient_cancel(IntPtr self);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void ControlObjectClient_setOrigin(IntPtr self, string orIdent, int orCat);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void ControlObjectClient_enableInterlockCheck(IntPtr self);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void ControlObjectClient_enableSynchroCheck(IntPtr self);

			private IntPtr controlObject;

			internal ControlObject (string objectReference, IntPtr connection)
			{
				this.controlObject = ControlObjectClient_create(objectReference, connection);

				if (this.controlObject == System.IntPtr.Zero)
					throw new IedConnectionException("Control object not found", 0);
			}

			~ControlObject ()
			{
				ControlObjectClient_destroy(controlObject);
			}

            /// <summary>
            /// Gets the control model.
            /// </summary>
            /// <returns>
            /// The control model.
            /// </returns>
			public ControlModel GetControlModel ()
			{
				ControlModel controlModel = (ControlModel) ControlObjectClient_getControlModel(controlObject);

				return controlModel;
			}

            /// <summary>
            /// Sets the origin parameter used by control commands.
            /// </summary>
            /// <param name='originator'>
            /// Originator. An arbitrary string identifying the controlling client.
            /// </param>
            /// <param name='originatorCategory'>
            /// Originator category.
            /// </param>
            public void SetOrigin (string originator, OrCat originatorCategory)
            {
                ControlObjectClient_setOrigin(controlObject, originator, (int) originatorCategory);
            }

            /// <summary>
            /// Operate the control with the specified control value.
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (bool ctlVal)
			{
				return Operate (ctlVal, 0);
			}

            /// <summary>
            /// Operate the control with the specified control value (time activated control).
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <param name='operTime'>the time when the operation will be executed</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (bool ctlVal, UInt64 operTime)
			{
				MmsValue value = new MmsValue(ctlVal);

				return Operate (value, operTime);
			}

            /// <summary>
            /// Operate the control with the specified control value.
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (float ctlVal)
			{
				return Operate (ctlVal, 0);
			}

            /// <summary>
            /// Operate the control with the specified control value (time activated control).
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <param name='operTime'>the time when the operation will be executed</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (float ctlVal, UInt64 operTime)
			{
				MmsValue value = new MmsValue(ctlVal);

				return Operate (value, operTime);
			}

            /// <summary>
            /// Operate the control with the specified control value.
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (int ctlVal)
			{
				return Operate (ctlVal, 0);
			}

            /// <summary>
            /// Operate the control with the specified control value (time activated control).
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <param name='operTime'>the time when the operation will be executed</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (int ctlVal, UInt64 operTime)
			{
				MmsValue value = new MmsValue(ctlVal);

				return Operate (value, operTime);
			}

            /// <summary>
            /// Operate the control with the specified control value.
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (MmsValue ctlVal)
			{
				return Operate (ctlVal, 0);
			}

            /// <summary>
            /// Operate the control with the specified control value (time activated control).
            /// </summary>
            /// <param name='ctlVal'>the new value of the control</param>
            /// <param name='operTime'>the time when the operation will be executed</param>
            /// <returns>true when the operation has been successful, false otherwise</returns>
			public bool Operate (MmsValue ctlVal, UInt64 operTime)
			{
				return ControlObjectClient_operate(controlObject, ctlVal.valueReference, operTime);
			}

            /// <summary>
            /// Select the control object.
            /// </summary>
            /// <returns>true when the selection has been successful, false otherwise</returns>
            public bool Select ()
            {
                return ControlObjectClient_select(controlObject);
            }


            /// <summary>
            /// Send a select with value command for generic MmsValue instances
            /// </summary>
            /// <param name='ctlVal'>
            /// the value to be checked.
            /// </param>
            /// <returns>true when the selection has been successful, false otherwise</returns>
            public bool SelectWithValue (MmsValue ctlVal)
            {
                return ControlObjectClient_selectWithValue(controlObject, ctlVal.valueReference);
            }

            /// <summary>
            /// Send a select with value command for boolean controls
            /// </summary>
            /// <param name='ctlVal'>
            /// the value to be checked.
            /// </param>
            /// <returns>true when the selection has been successful, false otherwise</returns>
            public bool SelectWithValue (bool ctlVal)
            {
                return SelectWithValue(new MmsValue(ctlVal));
            }

            /// <summary>
            /// Send a select with value command for integer controls
            /// </summary>
            /// <param name='ctlVal'>
            /// the value to be checked.
            /// </param>
            /// <returns>true when the selection has been successful, false otherwise</returns>
            public bool SelectWithValue (int ctlVal)
            {
                return SelectWithValue(new MmsValue(ctlVal));
            }

            /// <summary>
            /// Send a select with value command for float controls
            /// </summary>
            /// <param name='ctlVal'>
            /// the value to be checked.
            /// </param>
            /// <returns>true when the selection has been successful, false otherwise</returns>
            public bool SelectWithValue (float ctlVal)
            {
                return SelectWithValue(new MmsValue(ctlVal));
            }

            /// <summary>
            /// Cancel a selection or time activated operation
            /// </summary>
            /// <returns>true when the cancelation has been successful, false otherwise</returns>
            public bool Cancel () 
            {
                return ControlObjectClient_cancel(controlObject);
            }

            /// <summary>
            /// Enables the synchro check for operate commands
            /// </summary>
            public void EnableSynchroCheck ()
            {
                ControlObjectClient_enableSynchroCheck(controlObject);
            }

            /// <summary>
            /// Enables the interlock check for operate and select commands
            /// </summary>
            public void EnableInterlockCheck ()
            {
                ControlObjectClient_enableInterlockCheck(controlObject);
            }

		}

	}

}

