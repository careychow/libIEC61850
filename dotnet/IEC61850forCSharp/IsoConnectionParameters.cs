/*
 *  IsoConnectionParameters.cs
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

namespace IEC61850
{
    namespace Client
    {
        public enum AcseAuthenticationMechanism
        {
            /** don't use authentication */
            ACSE_AUTH_NONE = 0,
            /** use password authentication */
            ACSE_AUTH_PASSWORD = 1
        }

        /// <summary>
        /// Connection parameters associated with the ISO protocol layers (transport, session, presentation, ACSE)
        /// </summary>
        public class IsoConnectionParameters
        {
            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void IsoConnectionParameters_destroy(IntPtr self);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void IsoConnectionParameters_setRemoteApTitle(IntPtr self, string apTitle,
                int aeQualifier);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void IsoConnectionParameters_setRemoteAddresses(IntPtr self, UInt32 pSelector,
                UInt16 sSelector, UInt16 tSelector);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void IsoConnectionParameters_setLocalApTitle(IntPtr self, string apTitle,
                int aeQualifier);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void IsoConnectionParameters_setLocalAddresses(IntPtr self, UInt32 pSelector,
                UInt16 sSelector, UInt16 tSelector);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void IsoConnectionParameters_setAcseAuthenticationParameter(IntPtr self,
                IntPtr acseAuthParameter);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern IntPtr AcseAuthenticationParameter_create();

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void AcseAuthenticationParameter_destroy(IntPtr self);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void AcseAuthenticationParameter_setAuthMechanism(IntPtr self, int mechanism);

            [DllImport("iec61850", CallingConvention = CallingConvention.Cdecl)]
            private static extern void AcseAuthenticationParameter_setPassword(IntPtr self, string password);

            private IntPtr self;

            private IntPtr authParameter = IntPtr.Zero;

            internal IsoConnectionParameters(IntPtr self)
            {
                this.self = self;
            }

            ~IsoConnectionParameters()
            {
                if (authParameter != IntPtr.Zero)
                    AcseAuthenticationParameter_destroy(authParameter);

                IsoConnectionParameters_destroy(self);
            }

            /// <summary>
            /// Sets the remote ap title related parameters
            /// </summary>
            /// <param name='apTitle'>
            /// remote AP title.
            /// </param>
            /// <param name='aeQualifier'>
            /// remote AE qualifier.
            /// </param>
            public void SetRemoteApTitle(string apTitle, int aeQualifier)
            {
                IsoConnectionParameters_setRemoteApTitle(self, apTitle, aeQualifier);
            }

            /// <summary>
            /// Sets the remote addresses for ISO layers (transport, session, presentation)
            /// </summary>
            /// <param name='pSelector'>
            /// presentation layer address
            /// </param>
            /// <param name='sSelector'>
            /// session layer address
            /// </param>
            /// <param name='tSelector'>
            /// ISO COTP transport layer address
            /// </param>
            public void SetRemoteAddresses(UInt32 pSelector, UInt16 sSelector, UInt16 tSelector)
            {
                IsoConnectionParameters_setRemoteAddresses(self, pSelector, sSelector, tSelector);
            }

            /// <summary>
            /// Sets the local ap title related parameters
            /// </summary>
            /// <param name='apTitle'>
            /// local AP title.
            /// </param>
            /// <param name='aeQualifier'>
            /// local AE qualifier.
            /// </param>
            public void SetLocalApTitle(string apTitle, int aeQualifier)
            {
                IsoConnectionParameters_setLocalApTitle(self, apTitle, aeQualifier);
            }

            /// <summary>
            /// Sets the local addresses for ISO layers (transport, session, presentation)
            /// </summary>
            /// <param name='pSelector'>
            /// presentation layer address
            /// </param>
            /// <param name='sSelector'>
            /// session layer address
            /// </param>
            /// <param name='tSelector'>
            /// ISO COTP transport layer address
            /// </param>
            public void SetLocalAddresses(UInt32 pSelector, UInt16 sSelector, UInt16 tSelector)
            {
                IsoConnectionParameters_setLocalAddresses(self, pSelector, sSelector, tSelector);
            }

            /// <summary>
            /// Instruct ACSE layer to use password authentication.
            /// </summary>
            /// <param name='password'>
            /// Password that will be used to authenticate the client
            /// </param>
            public void UsePasswordAuthentication(string password)
            {
                if (authParameter == IntPtr.Zero)
                {
                    authParameter = AcseAuthenticationParameter_create();
                    AcseAuthenticationParameter_setAuthMechanism(authParameter,
                        (int) AcseAuthenticationMechanism.ACSE_AUTH_PASSWORD);
                    AcseAuthenticationParameter_setPassword(authParameter, password);
                    IsoConnectionParameters_setAcseAuthenticationParameter(self, authParameter);
                }
                else
                    throw new IedConnectionException("Authentication parameter already set");
            }
        }
    }
}