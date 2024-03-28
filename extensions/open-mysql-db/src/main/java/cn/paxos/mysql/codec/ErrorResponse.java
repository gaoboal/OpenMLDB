/*
 * Copyright 2022 paxos.cn.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package cn.paxos.mysql.codec;

import io.netty.channel.ChannelHandlerContext;

/**
 * This packet indicates that an error occurred.
 */
public class ErrorResponse extends AbstractMySqlPacket implements MysqlServerPacket {

	private final int errorNumber;
	private final byte[] sqlState;
	private final String message;

	public ErrorResponse(int sequenceId, int errorNumber, byte[] sqlState, String message) {
		super(sequenceId);
		this.errorNumber = errorNumber;
		this.sqlState = sqlState;
		this.message = message;
	}

	public int getErrorNumber() {
		return errorNumber;
	}

	public byte[] getSqlState() {
		return sqlState;
	}

	public String getMessage() {
		return message;
	}

	@Override
	public void accept(MysqlServerPacketVisitor visitor, ChannelHandlerContext ctx) {
		visitor.visit(this, ctx);
	}
}
